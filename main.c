/* main.c */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif

#include "SOIL.h"

typedef struct {
    unsigned char r, g, b;
} Rgb;

const Rgb BLACK = (Rgb) {0, 0, 0};
const Rgb WHITE = (Rgb) {255, 255, 255};

typedef struct {
    int width, height;
    Rgb* data;
} ImageRgb;

int kernel_x[] = {
	-1,  0, +1,
	-2,  0, +2,
	-1,  0, +1,
};

int kernel_y[] = {
	+1, +2, +1,
	 0,  0, 0,
	-1, -2, -1,
};

void load(char* name, ImageRgb* pic);
void validate();

void stylize(
	size_t start_x, size_t start_y, size_t end_x, size_t end_y,
	Rgb out[][end_x], Rgb edges[][end_x], Rgb in[][end_x]
);

void detect_edges(
	size_t width, size_t height, Rgb in[][width], Rgb out[][width],
	int threshold
);

int convolute(Rgb pixels[], int size, int kernel[]);
void init();
void draw();
void keyboard(unsigned char key, int x, int y);

int width, height;

// Texture identifiers.
GLuint tex[2];
ImageRgb imgs[2];
// Holds the index of the selected image (0 or 1).
// imgs[0] is the input image, while imgs[1] is the output image.
int sel;

// Carrega uma imagem para a struct Img
void load(char* name, ImageRgb* pic)
{
    int chan;
    pic->data = (Rgb*) SOIL_load_image(name, &pic->width, &pic->height, &chan, SOIL_LOAD_RGB);

    if(!pic->data) {
        printf( "SOIL loading error: '%s'\n", SOIL_last_result() );
        exit(1);
    }

    printf("Load   : %d x %d x %d\n", pic->width, pic->height, chan);
}

int main(int argc, char** argv)
{
    if(argc < 1) {
        printf("artistic [source image]\n");
        exit(1);
    }

	glutInit(&argc,argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);

    load(argv[1], &imgs[0]);
    width = imgs[0].width;
    height = imgs[0].height;
    // The input and output images must have the same sizes.
    imgs[1].width  = imgs[0].width;
    imgs[1].height = imgs[0].height;
	imgs[1].data = calloc(imgs[1].width * imgs[1].height, 3);

	glutInitWindowSize(width, height);
	glutCreateWindow("Artistic");
	glutDisplayFunc(draw);
	glutKeyboardFunc(keyboard);
	glMatrixMode(GL_PROJECTION);
	gluOrtho2D(0.0, width, height, 0.0);
	glMatrixMode(GL_MODELVIEW);

    printf("Source  : %s %d x %d\n", argv[1], imgs[0].width, imgs[0].height);
    sel = 0;

    // Interpret the images as rgb matrixes.
    Rgb (*in)[width] = (Rgb(*)[width]) imgs[0].data;
    Rgb (*out)[width] = (Rgb(*)[width]) imgs[1].data;
	Rgb edges[height][width];

	detect_edges(width, height, in, edges, 800);
	stylize(0, 0, width, height, out, edges, in);

    tex[0] = SOIL_create_OGL_texture((unsigned char*) imgs[0].data, width, height, SOIL_LOAD_RGB, SOIL_CREATE_NEW_ID, 0);
    tex[1] = SOIL_create_OGL_texture((unsigned char*) imgs[1].data, width, height, SOIL_LOAD_RGB, SOIL_CREATE_NEW_ID, 0);

    glutMainLoop();
}

void stylize(
	size_t start_x, size_t start_y, size_t end_x, size_t end_y,
	Rgb out[][end_x], Rgb edges[][end_x], Rgb in[][end_x]
) {
	unsigned char first_color = edges[start_y][start_x].r;

	// Check if there is any edge in this sector of the image.
	// If so, divide it further into four sectors,
	// unless it's already one pixel wide and high.
	for (size_t row = start_y; row < end_y; row++) {
		for (size_t col = start_x; col < end_x; col++) {
			if (edges[row][col].r != first_color) {
				size_t middle_x = (start_x + end_x) / 2;
				size_t middle_y = (start_y + end_y) / 2;

				stylize(start_x, start_y, middle_x, middle_y, out, edges, in);
				stylize(middle_x + 1, start_y, end_x, middle_y, out, edges, in);
				stylize(start_x, middle_y + 1, middle_x + 1, end_y, out, edges, in);
				stylize(middle_x + 1, middle_y + 1, end_x, end_y, out, edges, in);

				return;
			}
		}
	}

	Rgb color = in[(start_y + end_y) / 2][(start_x + end_x) / 2];

	for (size_t row = start_y; row < end_y; row++) {
		for (size_t col = start_x; col < end_x; col++) {
			out[row][col] = color;
		}
	}
}

void detect_edges(
	size_t width, size_t height, Rgb in[][width], Rgb out[][width],
	int threshold
) {
	for (size_t row = 1; row < height-1; row++) {
		for (size_t col = 1; col < width-1; col++) {
			Rgb neighbors[] = {
				in[row-1][col-1],
				in[row-1][col],
				in[row-1][col+1],
				in[row][col-1],
				in[row][col],
				in[row][col+1],
				in[row+1][col-1],
				in[row+1][col],
				in[row+1][col+1],
			};

			int Gx = convolute(neighbors, 9, kernel_x);
			int Gy = convolute(neighbors, 9, kernel_y);
			int G = sqrt(Gx*Gx + Gy*Gy);
			out[row][col] = G < threshold ? BLACK : WHITE;
		}
	}
}

int convolute(Rgb pixels[], int size, int kernel[]) {
	int result = 0;

	for (size_t i = 0; i < size; i++) {
		result += pixels[i].r * kernel[i];
		result += pixels[i].g * kernel[i];
		result += pixels[i].b * kernel[i];
	}

	return result;
}

void keyboard(unsigned char key, int x, int y)
{
	// Listen for the ESC key.
	if (key==27) {
		free(imgs[0].data);
		free(imgs[1].data);
		exit(1);
	}

	if (key >= '1' && key <= '2') {
		sel = key - '1';
	}

	glutPostRedisplay();
}

void draw()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glColor3ub(255, 255, 255);

    // Activate the texture that corresponds to the selected image.
    glBindTexture(GL_TEXTURE_2D, tex[sel]);
	// Draw a rectangle that occupies the whole screen.
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);

    glTexCoord2f(0,0);
    glVertex2f(0,0);

    glTexCoord2f(1,0);
    glVertex2f(imgs[sel].width,0);

    glTexCoord2f(1,1);
    glVertex2f(imgs[sel].width, imgs[sel].height);

    glTexCoord2f(0,1);
    glVertex2f(0,imgs[sel].height);

    glEnd();
    glDisable(GL_TEXTURE_2D);

	// Display the image.
    glutSwapBuffers();
}

