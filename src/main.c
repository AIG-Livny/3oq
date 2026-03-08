/* Freetype GL - A C OpenGL Freetype engine
 *
 * Distributed under the OSI-approved BSD 2-Clause License.  See accompanying
 * file `LICENSE` for more details.
 */


#include <stdio.h>
#include <string.h>
#include <fontconfig/fontconfig.h>
#include <wchar.h>
#include <locale.h>

#include "shaderutils.h"
#include "freetype-gl.h"
#include "vertex-buffer.h"
#include "text-buffer.h"
#include "font-manager.h"
#include "iso646.h"
#include "unistd.h"
#include "cgeom.h"

#include <GLFW/glfw3.h>


char* match_description(const char* description ) {
    char *filename = 0;
    FcInit();
    FcPattern *pattern = FcNameParse((FcChar8*)description);
    FcConfigSubstitute( 0, pattern, FcMatchPattern );
    FcDefaultSubstitute( pattern );
    FcResult result;
    FcPattern *match = FcFontMatch( 0, pattern, &result );
    FcPatternDestroy( pattern );

    if ( !match )
    {
        fprintf( stderr, "fontconfig error: could not match description '%s'", description );
        return 0;
    }
    else
    {
        FcValue value;
        FcResult result = FcPatternGet( match, FC_FILE, 0, &value );
        if ( result )
        {
            fprintf( stderr, "fontconfig error: could not match description '%s'", description );
        }
        else
        {
            filename = strdup( (char *)(value.u.s) );
        }
    }
    FcPatternDestroy( match );
    return filename;
}

// ------------------------------------------------------- typedef & struct ---
typedef struct {
    float x, y, z;    // position
    float s, t;       // texture
    float r, g, b, a; // color
} vertex_t;


// ------------------------------------------------------- global variables ---
texture_atlas_t * atlas;
texture_font_t * font;
vertex_buffer_t * buffer;
int line_count = 42;
GLuint text_shader;
GLuint bounds_shader;
struct mat4f model, view, projection;


// --------------------------------------------------------------- add_text ---
void add_text( vertex_buffer_t * buffer, texture_font_t * font,
               char *text, struct vec4f* color, struct vec2f* pen )
{
    size_t i;
    float r = color->red, g = color->green, b = color->blue, a = color->alpha;
    for( i = 0; i < strlen(text); ++i )
    {
        texture_glyph_t *glyph = texture_font_get_glyph( font, text + i );
        if( glyph != NULL )
        {
            float kerning = 0.0f;
            if( i > 0)
            {
                kerning = texture_glyph_get_kerning( glyph, text + i - 1 );
            }
            pen->x += kerning;
            int x0  = (int)( pen->x + glyph->offset_x );
            int y0  = (int)( pen->y + glyph->offset_y );
            int x1  = (int)( x0 + glyph->width );
            int y1  = (int)( y0 - glyph->height );
            float s0 = glyph->s0;
            float t0 = glyph->t0;
            float s1 = glyph->s1;
            float t1 = glyph->t1;
            GLuint index = buffer->vertices->size;
            GLuint indices[] = {index, index+1, index+2,
                                index, index+2, index+3};
            vertex_t vertices[] = { { x0,y0,0,  s0,t0,  r,g,b,a },
                                    { x0,y1,0,  s0,t1,  r,g,b,a },
                                    { x1,y1,0,  s1,t1,  r,g,b,a },
                                    { x1,y0,0,  s1,t0,  r,g,b,a } };
            vertex_buffer_push_back_indices( buffer, indices, 6 );
            vertex_buffer_push_back_vertices( buffer, vertices, 4 );
            pen->x += glyph->advance_x;
        }
    }
}

void reshape( GLFWwindow* window, int width, int height ) {
    glViewport(0, 0, width, height);
    mat4f_set_ortho(&projection, 0, width, 0, height, -1, 1);
}

void keyboard( GLFWwindow* window, int key, int scancode, int action, int mods ) {
    if ( key == GLFW_KEY_ESCAPE && action == GLFW_PRESS ) {
        glfwSetWindowShouldClose( window, GL_TRUE );
    }
}

void error_callback( int error, const char* description ) {
    fputs( description, stderr );
}

markup_t n;
struct vec2f pen;
font_manager_t* font_manager;
text_buffer_t* text_buffer;

void update_buffer(){
        glGenTextures( 1, &font_manager->atlas->id );
        glBindTexture( GL_TEXTURE_2D, font_manager->atlas->id );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, font_manager->atlas->width,
                      font_manager->atlas->height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                      font_manager->atlas->data );

}

void prin(const char* str){
    text_buffer_printf( text_buffer, &pen,
                        &n,   str, NULL);
    update_buffer();
}

void codepoint_to_utf8(unsigned int cp, char* out) {
    if (cp <= 0x7F) {
        out[0] = (char)cp;
        out[1] = '\0';
    } else if (cp <= 0x7FF) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        out[2] = '\0';
    } else if (cp <= 0xFFFF) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        out[3] = '\0';
    } else if (cp <= 0x10FFFF) {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        out[4] = '\0';
    } else {
        out[0] = '\0'; // Некорректный символ
    }
}

void char_input( GLFWwindow* window, unsigned int codepoint) {
    char utf8_string[5];
    codepoint_to_utf8(codepoint, utf8_string);
    text_buffer_add_char( text_buffer, &pen, &n, utf8_string, 0 );
    update_buffer();
}

int main( int argc, char **argv ) {
    glfwSetErrorCallback( error_callback );

    if ( not glfwInit() ) {
        exit( EXIT_FAILURE );
    }

    glfwWindowHint( GLFW_VISIBLE, GL_FALSE );
    glfwWindowHint( GLFW_RESIZABLE, GL_TRUE );

    GLFWwindow* window = glfwCreateWindow( 800, 600, argv[0], NULL, NULL );
    if ( not window ) {
        glfwTerminate();
        exit( EXIT_FAILURE );
    }

    setlocale(LC_ALL, "ru_RU.UTF-8");

    glfwMakeContextCurrent( window );
    //glfwSwapInterval(0);

    glfwSetFramebufferSizeCallback( window, reshape );
    //glfwSetWindowRefreshCallback( window, display );
    glfwSetKeyCallback( window, keyboard );
    glfwSetCharCallback(window, char_input);


    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if ( GLEW_OK != err && err != 4 ){
        fprintf( stderr, "Error: %s\n", glewGetErrorString(err) );
        exit( EXIT_FAILURE );
    }

    fprintf( stderr, "Using GLEW %s\n", glewGetString(GLEW_VERSION) );

    // ------------------------------------------------------------------- init ---
    #define PATH "lib/freetype-gl/"

    su_load_vert_frag(&text_shader,PATH"shaders/text.vert",
                              PATH"shaders/text.frag" );

    text_buffer = text_buffer_new();

    font_manager = font_manager_new( 512, 512, LCD_FILTERING_ON );
    //text_buffer_t* buffer = text_buffer_new( );

    struct vec4f black  = {0.0, 0.0, 0.0, 1.0};
    struct vec4f white  = {1.0, 1.0, 1.0, 1.0};
    struct vec4f yellow = {1.0, 1.0, 0.0, 1.0};
    struct vec4f grey   = {0.5, 0.5, 0.5, 1.0};
    struct vec4f none   = {1.0, 1.0, 1.0, 0.0};

    //char *family   = match_description("Droid Serif:size=24");
    n = (struct markup_t){
        .family  = PATH"fonts/SourceSansPro-Regular.ttf",
        .size    = 24.0, .bold    = 0,   .italic  = 0,
        .spacing = 0.0,  .gamma   = 2.,
        .foreground_color    = white, .background_color    = none,
        .underline           = 0,     .underline_color     = white,
        .overline            = 0,     .overline_color      = white,
        .strikethrough       = 0,     .strikethrough_color = white,
        .font = 0,
    };

    n.font = font_manager_get_from_markup( font_manager, &n );
    pen.x = 20;
    pen.y=200;

    prin(" gg джигурда");
    prin(" the lazy dog\n");
    prin(" адругqwertyuii");
    prin("п");


    float left   = 0;
    float right  = 800;
    float top    = 0;
    float bottom = 600;

    su_load_vert_frag(&bounds_shader, PATH"shaders/v3f-c4f.vert",
                                 PATH"shaders/v3f-c4f.frag" );

    vertex_buffer_t* lines_buffer = vertex_buffer_new( "vertex:3f,color:4f" );
    vertex_t vertices[] = { { left - 10,         top, 0, 0,0,0,1}, // top
                            {right + 10,         top, 0, 0,0,0,1},

                            { left - 10,      bottom, 0, 0,0,0,1}, // bottom
                            {right + 10,      bottom, 0, 0,0,0,1},

                            {      left,    top + 10, 0, 0,0,0,1}, // left
                            {      left, bottom - 10, 0, 0,0,0,1},
                            {     right,    top + 10, 0, 0,0,0,1}, // right
                            {     right, bottom - 10, 0, 0,0,0,1} };
    GLuint indices[] = { 0,1,2,3,4,5,6,7 };
    vertex_buffer_push_back( lines_buffer, vertices, 8, indices, 8);

    mat4f_set_identity( &projection );
    mat4f_set_identity( &model );
    mat4f_set_identity( &view );


    glfwShowWindow( window );
    reshape( window, 800, 600 );

    glfwSetTime(0.0);

    while ( not glfwWindowShouldClose( window ) ) {
        glfwWaitEvents();

        glClearColor(0.40,0.40,0.45,1.00);
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

        glColor4f(1.00,1.00,1.00,1.00);
        glUseProgram( text_shader );
        {
            glUniformMatrix4fv( glGetUniformLocation( text_shader, "model" ),
                                1, 0, model.v);
            glUniformMatrix4fv( glGetUniformLocation( text_shader, "view" ),
                                1, 0, view.v);
            glUniformMatrix4fv( glGetUniformLocation( text_shader, "projection" ),
                                1, 0, projection.v);
            glUniform1i( glGetUniformLocation( text_shader, "tex" ), 0 );
            glUniform3f( glGetUniformLocation( text_shader, "pixel" ),
                         1.0f/font_manager->atlas->width,
                         1.0f/font_manager->atlas->height,
                         (float)font_manager->atlas->depth );

            glEnable( GL_BLEND );

            glActiveTexture( GL_TEXTURE0 );
            glBindTexture( GL_TEXTURE_2D, font_manager->atlas->id );

            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

            vertex_buffer_render( text_buffer->buffer, GL_TRIANGLES );
            glBindTexture( GL_TEXTURE_2D, 0 );
            glUseProgram( 0 );
        }

        glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
        glUseProgram( bounds_shader );
        {
            glUniformMatrix4fv( glGetUniformLocation( bounds_shader, "model" ),
                                1, 0, model.v);
            glUniformMatrix4fv( glGetUniformLocation( bounds_shader, "view" ),
                                1, 0, view.v);
            glUniformMatrix4fv( glGetUniformLocation( bounds_shader, "projection" ),
                                1, 0, projection.v);
            vertex_buffer_render( lines_buffer, GL_LINES );
        }

        glfwSwapBuffers( window );

        //glfwPollEvents( );
        //sleep(1);

    }

    //glDeleteTextures( 1, &atlas->id );
    //atlas->id = 0;
    //texture_atlas_delete( atlas );

    glfwDestroyWindow( window );
    glfwTerminate( );

    return EXIT_SUCCESS;
}