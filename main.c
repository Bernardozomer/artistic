/* main.c */
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

typedef struct {
    int width, height;
    Rgb* data;
} ImageRgb;

typedef struct point {
	size_t x;
	size_t y;
} Point;

static const Rgb BLACK = (Rgb) {0, 0, 0};
static const Rgb WHITE = (Rgb) {255, 255, 255};

void load(char* name, ImageRgb* pic);
void validate();

void stylize(
	size_t width, size_t height, Rgb in[][width], Rgb out[][width], Point* seeds
);

void find_seeds(
	Point* seeds, size_t width, size_t height, Rgb edges[][width]
);

static void find_seeds_step(
	Point* seeds,
	size_t start_x, size_t start_y,
	size_t end_x, size_t end_y,
	Rgb edges[][end_x]
);

void detect_edges(
	size_t width, size_t height,
	Rgb in[][width], Rgb out[][width],
	int threshold
);

static int convolute(Rgb pixels[], int size, const int kernel[]);
void init();
void draw();
void keyboard(unsigned char key, int x, int y);

int width, height;
size_t max_seeds = 80000;
size_t seeds_found = 0;

// Texture identifiers.
GLuint tex[2];
ImageRgb imgs[2];
// Holds the index of the selected image (0 or 1).
// imgs[0] is the input image, while imgs[1] is the output image.
int sel;

int main(int argc, char** argv)
{
    if(argc < 3) {
        printf("artistic [source image] [edge detection threshold]\n");
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

    printf("Source      : %s %d x %d\n", argv[1], imgs[0].width, imgs[0].height);
    sel = 0;

    // Interpret the images as rgb matrixes.
    Rgb (*in)[width] = (Rgb(*)[width]) imgs[0].data;
    Rgb (*out)[width] = (Rgb(*)[width]) imgs[1].data;

	Rgb edges[height][width];
	detect_edges(width, height, in, edges, atol(argv[2]));

	Point seeds[max_seeds];
	find_seeds(seeds, width, height, edges);
	printf("Seeds found : %zu\n", seeds_found);

	stylize(width, height, in, out, seeds);

    tex[0] = SOIL_create_OGL_texture(
		(unsigned char*) imgs[0].data, width, height,
		SOIL_LOAD_RGB, SOIL_CREATE_NEW_ID, 0
	);

    tex[1] = SOIL_create_OGL_texture(
		(unsigned char*) imgs[1].data, width, height,
		SOIL_LOAD_RGB, SOIL_CREATE_NEW_ID, 0
	);

    glutMainLoop();
}

/**
 * Stylize the given image based on its seeds into a voronoi diagram
 * using the euclidean distance as a metric.
 *
 * Performs badly because every pixel calculates its distance to every seed.
 */
void stylize(
	size_t width, size_t height, Rgb in[][width], Rgb out[][width], Point* seeds
) {
	for (size_t row = 0; row < height; row++) {
		for (size_t col = 0; col < width; col++) {
			Point closest_seed = seeds[0];
			int closest_dist = INT32_MAX;

			for (size_t i = 0; i < seeds_found; i++) {
				Point seed = seeds[i];

				int dx = (int) col - (int) seed.x;
				int dy = (int) row - (int) seed.y;
				int dist = dx*dx + dy*dy;

				if (dist < closest_dist) {
					closest_seed = seed;
					closest_dist = dist;
				}
			}

			out[row][col] = in[closest_seed.y][closest_seed.x];
		}
	}
}

/**
 * Start the process of finding seeds on a given image based on its edges.
 *
 * This function exists to provide a simpler interface for the user.
 */
void find_seeds(
	Point* seeds, size_t width, size_t height, Rgb edges[][width]
) {
	find_seeds_step(seeds, 0, 0, width, height, edges);
}

/**
 * Find spots appropriate for seeds that will be later used
 * to stylize an image.
 *
 * This function behaves like a quadtree, subdividing the space into
 * four quadrants each time it doesn't satisfy a condition.
 * In this case, that means being an area of pixels that are
 * all edges or all not edges.
 */
static void find_seeds_step(
	Point* seeds,
	size_t start_x, size_t start_y,
	size_t end_x, size_t end_y,
	Rgb edges[][end_x]
) {
	unsigned char first_color = edges[start_y][start_x].r;
	size_t middle_x = (start_x + end_x) / 2;
	size_t middle_y = (start_y + end_y) / 2;

	for (size_t row = start_y; row < end_y; row++) {
		for (size_t col = start_x; col < end_x; col++) {
			// Check if this sector of the image is
			// all edges or all not edges.
			if (edges[row][col].r == first_color) {
				continue;
			}

			// If not, divide it further into four sectors.
			size_t boundaries[][4] = {
				{ start_x,      start_y,      middle_x, middle_y },
				{ middle_x + 1, start_y,      end_x,    middle_y },
				{ start_x,      middle_y + 1, middle_x, end_y },
				{ middle_x + 1, middle_y + 1, end_x,    end_y }
			};

			for (size_t i = 0; i < 4; i++) {
				if (seeds_found == max_seeds) {
					break;
				}

				find_seeds_step(
					seeds,
					boundaries[i][0],
					boundaries[i][1],
					boundaries[i][2],
					boundaries[i][3],
					edges
				);
			}

			return;
		}
	}

	// If so, place a seed in this spot.
	seeds[seeds_found] = (Point) { middle_x, middle_y };
	seeds_found++;
}

/**
 * Perform sobel edge detection on the given image.
 *
 * Unlike a standard implementation, this function transforms
 * gradient edge values into binary ones.
 *
 * This is done to simplify further processing and is based
 * on the given threshold.
 *
 * See also: https://en.wikipedia.org/wiki/Sobel_operator
 *
 * args:
 * threshold: the higher it is, the less edges will be found.
 *			  should be a number between 0 and 4327.
 */
void detect_edges(
	size_t width, size_t height,
	Rgb in[][width], Rgb out[][width],
	int threshold
) {
	static const int kernel_x[] = {
		-1,  0, +1,
		-2,  0, +2,
		-1,  0, +1,
	};

	static const int kernel_y[] = {
		+1, +2, +1,
		 0,  0, 0,
		-1, -2, -1,
	};

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

/**
 * Compute the result of the sobel operator
 * for a given point of the given image.
 *
 * See also: https://en.wikipedia.org/wiki/Sobel_operator
 */
static int convolute(Rgb pixels[], int size, const int kernel[]) {
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

void load(char* name, ImageRgb* pic)
{
    int chan;
    pic->data = (Rgb*) SOIL_load_image(
		name, &pic->width, &pic->height, &chan, SOIL_LOAD_RGB
	);

    if(!pic->data) {
        printf( "SOIL loading error: '%s'\n", SOIL_last_result() );
        exit(1);
    }

    printf("Load        : %d x %d x %d\n", pic->width, pic->height, chan);
}

