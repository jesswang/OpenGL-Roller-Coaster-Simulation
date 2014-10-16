/*
  CSCI 480
  Assignment 2
 */

#include <stdio.h>
#include <stdlib.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#include <vector>
#include "pic.h"
#include "math.h"

/* represents one control point along the spline */
struct point {
   double x;
   double y;
   double z;
};

/* spline struct which contains how many control points, and an array of control points */
struct spline {
   int numControlPoints;
   struct point *points;
};

/* the spline array */
struct spline *g_Splines;

/* total number of splines */
int g_iNumOfSplines;

std::vector<struct point*> spline_points;
std::vector<struct point*> tangent_vectors;

int g_vMousePos[2] = {0, 0};
int g_iLeftMouseButton = 0;    /* 1 if pressed, 0 if not */
int g_iMiddleMouseButton = 0;
int g_iRightMouseButton = 0;

typedef enum { ROTATE, TRANSLATE, SCALE } CONTROLSTATE;

CONTROLSTATE g_ControlState = ROTATE;

/* state of the world */
float g_vLandRotate[3] = {0.0, 0.0, 0.0};
float g_vLandTranslate[3] = {0.0, 0.0, 0.0};
float g_vLandScale[3] = {1.0, 1.0, 1.0};

double eye_x = 0.0, eye_y = 0.0, eye_z = 0.0, center_x = 0.0, center_y = 0.0, center_z = 0.0, up_x = 0.0, up_y = 0.0, up_z = 0.0;

GLuint textureSky[1];

void texload(int i, char *filename) 
{ 
  Pic* img; 
  img = jpeg_read(filename, NULL); 
  glBindTexture(GL_TEXTURE_2D, textureSky[i]);
  glTexImage2D(GL_TEXTURE_2D, 
    0, 
    GL_RGB, 
    img->nx, 
    img->ny, 
    0, 
    GL_RGB, 
    GL_UNSIGNED_BYTE, 
    &img->pix[0]); 
  pic_free(img); 
} 

int loadSplines(char *argv) {
  char *cName = (char *)malloc(128 * sizeof(char));
  FILE *fileList;
  FILE *fileSpline;
  int iType, i = 0, j, iLength;


  /* load the track file */
  fileList = fopen(argv, "r");
  if (fileList == NULL) {
    printf ("can't open file\n");
    exit(1);
  }
  
  /* stores the number of splines in a global variable */
  fscanf(fileList, "%d", &g_iNumOfSplines);

  g_Splines = (struct spline *)malloc(g_iNumOfSplines * sizeof(struct spline));

  /* reads through the spline files */
  for (j = 0; j < g_iNumOfSplines; j++) {
    i = 0;
    fscanf(fileList, "%s", cName);
    fileSpline = fopen(cName, "r");

    if (fileSpline == NULL) {
      printf ("can't open file\n");
      exit(1);
    }

    /* gets length for spline file */
    fscanf(fileSpline, "%d %d", &iLength, &iType);

    /* allocate memory for all the points */
    g_Splines[j].points = (struct point *)malloc(iLength * sizeof(struct point));
    g_Splines[j].numControlPoints = iLength;

    /* saves the data to the struct */
    while (fscanf(fileSpline, "%lf %lf %lf", 
	   &g_Splines[j].points[i].x, 
	   &g_Splines[j].points[i].y, 
	   &g_Splines[j].points[i].z) != EOF) {
      i++;
    }
  }
  //printf("Num of control points %d\n", iLength);
  free(cName);

  return 0;
}

double catmullRomPoint(double p1, double p2, double p3, double p4, double u)
{
  double v1 = 1.0 * p2;
  double v2 = -0.5 * p1 + 0.5 * p3;
  double v3 = 1.0 * p1 - 2.5 * p2 + 2.0 * p3 - 0.5 * p4;
  double v4 = -0.5 * p1 + 1.5 * p2 - 1.5 * p3 + 0.5 * p4;
  //return (((v4 * u + v3) * u + v2) * u + v1);
  return v4 * pow(u, 3) + v3 * pow(u, 2) + v2 * u + v1;
}

double computeTangent(double p1, double p2, double p3, double p4, double u)
{
  double v1 = 1.0 * p2;
  double v2 = -0.5 * p1 + 0.5 * p3;
  double v3 = 1.0 * p1 - 2.5 * p2 + 2.0 * p3 - 0.5 * p4;
  double v4 = -0.5 * p1 + 1.5 * p2 - 1.5 * p3 + 0.5 * p4;
  return v4 * 3 * pow(u, 2) + v3 * 2 * u + v2;
}

struct point* computeCrossProduct(struct point* v1, struct point* v2)
{
  struct point* n = (struct point*) malloc(sizeof(struct point));
  n->x = v1->y * v2->z - v1->z * v2->y;
  n->y = v1->z * v2->x - v1->x * v2->z;
  n->z = v1->x * v2->y - v1->y * v2->x;
  return n;
}

struct point* unit(struct point* v)
{
  //struct point* n = (struct point*) malloc(sizeof(struct point));
  double magnitude = sqrt(pow(v->x, 2) + pow(v->y, 2) + pow(v->z, 2));
  v->x /= magnitude;
  v->y /= magnitude;
  v->z /= magnitude;
  return v;
}

void simulateCoasterRide()
{
  for(int i = 0; i < tangent_vectors.size(); ++i)
  {    
    if(i % 500 == 0)
    {
      glBegin(GL_LINES);
        glVertex3d(tangent_vectors[i]->x, tangent_vectors[i]->y, tangent_vectors[i]->z);
        glVertex3d(spline_points[i]->x, spline_points[i]->y, spline_points[i]->z);
      glEnd();
    }
    /*eye_x = *it->x;
    eye_y = *it->y;
    eye_z = *it->z;*/
  }
}

void drawSplines()
{
  glBegin(GL_LINE_STRIP);
  for(int i = 0; i < g_iNumOfSplines; ++i)
  {
    int index = 0;
    for(int j = 1; j + 2 < g_Splines[i].numControlPoints; ++j)
    {
      struct point* binormal_vec = (struct point*) malloc(sizeof(struct point));
      for(double u = 0.0; u < 1.0; u += 0.001)
      {
        struct point* catmull_vec = (struct point*) malloc(sizeof(struct point));
        struct point* tangent_vec = (struct point*) malloc(sizeof(struct point));
        struct point* normal_vec = (struct point*) malloc(sizeof(struct point));

        catmull_vec->x = catmullRomPoint(g_Splines[i].points[j-1].x, g_Splines[i].points[j].x, g_Splines[i].points[j+1].x, g_Splines[i].points[j+2].x, u);
        catmull_vec->y = catmullRomPoint(g_Splines[i].points[j-1].y, g_Splines[i].points[j].y, g_Splines[i].points[j+1].y, g_Splines[i].points[j+2].y, u);
        catmull_vec->z = catmullRomPoint(g_Splines[i].points[j-1].z, g_Splines[i].points[j].z, g_Splines[i].points[j+1].z, g_Splines[i].points[j+2].z, u);
        spline_points.push_back(catmull_vec);

        tangent_vec->x = computeTangent(g_Splines[i].points[j-1].x, g_Splines[i].points[j].x, g_Splines[i].points[j+1].x, g_Splines[i].points[j+2].x, u);
        tangent_vec->y = computeTangent(g_Splines[i].points[j-1].y, g_Splines[i].points[j].y, g_Splines[i].points[j+1].y, g_Splines[i].points[j+2].y, u);
        tangent_vec->z = computeTangent(g_Splines[i].points[j-1].z, g_Splines[i].points[j].z, g_Splines[i].points[j+1].z, g_Splines[i].points[j+2].z, u);
        tangent_vec = unit(tangent_vec);
        tangent_vectors.push_back(tangent_vec);

        if(u == 0.0)
        {
          binormal_vec->x = tangent_vec->y*-1;
          binormal_vec->y = tangent_vec->x;
          binormal_vec->z = tangent_vec->z;
          binormal_vec = unit(binormal_vec);
        }

        //compute normal and normalize it
        normal_vec = unit(computeCrossProduct(tangent_vec, binormal_vec));

        binormal_vec = unit(computeCrossProduct(tangent_vec, normal_vec));

        eye_x = catmull_vec->x;
        eye_y = catmull_vec->y;
        eye_z = catmull_vec->z;
        center_x = eye_x + u*tangent_vec->x;
        center_y = eye_y + u*tangent_vec->y;
        center_z = eye_z + u*tangent_vec->z;
        up_x = 0.0;
        up_y = 0.0;
        up_z = 1.0;

        glVertex3d(catmull_vec->x, catmull_vec->y, catmull_vec->z);

        if(index % 500 == 0)
        {
          //glVertex3d(normal_vec->x, normal_vec->y, normal_vec->z);
        }
        //printf("%f, %f, %f\n", up_x, up_y, up_z);
        
        ++index;
        free(tangent_vec);
        free(normal_vec);
      }
      free(binormal_vec);
    }
  }
  glEnd();
  /*for(int i = 0; i < g_iNumOfSplines; ++i)
  {
    for(int k = 0; k < (g_Splines[i].numControlPoints - 2)*1000; ++k)
    {
      if(k % 500 == 0)
      {
        glBegin(GL_LINES);
        glVertex3d(g_Splines[i].catmull_rom_points[k].x, g_Splines[i].catmull_rom_points[k].y, g_Splines[i].catmull_rom_points[k].z);
        glVertex3d(g_Splines[i].tangent_vectors[k].x, g_Splines[i].tangent_vectors[k].y, g_Splines[i].tangent_vectors[k].z);
        glEnd();
      }
    }
  }*/
}

void drawGround()
{
  texload(0,const_cast<char *>("waterfall.jpg"));
  glBegin(GL_POLYGON);
  glTexCoord2f(1.0f, 0.0f); glVertex3f(-100.0f, -100.0f, -30.0f); 
  glTexCoord2f(1.0f, 1.0f); glVertex3f(-100.0f,  100.0f, -30.0f);  
  glTexCoord2f(0.0f, 1.0f); glVertex3f( 100.0f,  100.0f, -30.0f);  
  glTexCoord2f(0.0f, 0.0f); glVertex3f( 100.0f, -100.0f, -30.0f);
  glEnd();
}

void drawSkyBox()
{
  //front
  texload(0,const_cast<char *>("sky_08.jpg"));
  glBegin(GL_QUADS);
  glTexCoord2f(1.0f, 1.0f); glVertex3f(-100.0f, -100.0f, -100.0f); 
  glTexCoord2f(0.0f, 1.0f); glVertex3f( 100.0f, -100.0f, -100.0f);  
  glTexCoord2f(0.0f, 0.0f); glVertex3f( 100.0f, -100.0f,  100.0f);  
  glTexCoord2f(1.0f, 0.0f); glVertex3f(-100.0f, -100.0f,  100.0f);  
  glEnd();

  //back
  texload(0,const_cast<char *>("sky_06.jpg"));
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 1.0f); glVertex3f(-100.0f,  100.0f, -100.0f);  
  glTexCoord2f(0.0f, 0.0f); glVertex3f(-100.0f,  100.0f,  100.0f); 
  glTexCoord2f(1.0f, 0.0f); glVertex3f( 100.0f,  100.0f,  100.0f); 
  glTexCoord2f(1.0f, 1.0f); glVertex3f( 100.0f,  100.0f, -100.0f);  
  glEnd();

  //top
  texload(0,const_cast<char *>("sky_02.jpg"));
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f); glVertex3f(-100.0f, -100.0f,  100.0f);  
  glTexCoord2f(1.0f, 0.0f); glVertex3f( 100.0f, -100.0f,  100.0f);  
  glTexCoord2f(1.0f, 1.0f); glVertex3f( 100.0f,  100.0f,  100.0f);  
  glTexCoord2f(0.0f, 1.0f); glVertex3f(-100.0f,  100.0f,  100.0f);  
  glEnd();

  //right
  texload(0,const_cast<char *>("sky_07.jpg"));
  glBegin(GL_QUADS);
  glTexCoord2f(1.0f, 1.0f); glVertex3f( 100.0f, -100.0f, -100.0f);  
  glTexCoord2f(0.0f, 1.0f); glVertex3f( 100.0f,  100.0f, -100.0f);  
  glTexCoord2f(0.0f, 0.0f); glVertex3f( 100.0f,  100.0f,  100.0f);  
  glTexCoord2f(1.0f, 0.0f); glVertex3f( 100.0f, -100.0f,  100.0f);  
  glEnd();

  //left
  texload(0,const_cast<char *>("sky_05.jpg"));
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 1.0f); glVertex3f(-100.0f, -100.0f, -100.0f);  
  glTexCoord2f(0.0f, 0.0f); glVertex3f(-100.0f, -100.0f,  100.0f);  
  glTexCoord2f(1.0f, 0.0f); glVertex3f(-100.0f,  100.0f,  100.0f);  
  glTexCoord2f(1.0f, 1.0f); glVertex3f(-100.0f,  100.0f, -100.0f);
  glEnd();
}

void init()
{
  glClearColor(0.0, 0.0, 0.0, 0.0);   
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(60.0, 600.0/600.0, 0.01, 1000.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt(-15.0, -15.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
  glEnable(GL_DEPTH_TEST);  
  glLineWidth(2.0);        
  glGenTextures(1, textureSky);
}

void display()
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glPushMatrix();
  drawSplines();
  //glBindTexture(GL_TEXTURE_2D, texture_bottom[0]); 
  glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE); 
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); 
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR); 

  glEnable(GL_TEXTURE_2D);
  drawGround();
  drawSkyBox();
  glDisable(GL_TEXTURE_2D);
  glPopMatrix();

  //simulateCoasterRide();
  //gluLookAt(eye_x, eye_y, eye_z, center_x, center_y, center_z, up_x, up_y, up_z);
  glutSwapBuffers();
}

/* converts mouse drags into information about
 rotation/translation/scaling */
void mousedrag(int x, int y)
{
    int vMouseDelta[2] = {x-g_vMousePos[0], y-g_vMousePos[1]};
    switch (g_ControlState)
    {
        case TRANSLATE:
            if (g_iLeftMouseButton)
            {
                g_vLandTranslate[0] += vMouseDelta[0]*0.01;
                g_vLandTranslate[1] -= vMouseDelta[1]*0.01;
                glTranslatef(g_vLandTranslate[0], g_vLandTranslate[1], 0.0); // translate w/ respect to x- and y- axes
            }
            if (g_iMiddleMouseButton)
            {
                g_vLandTranslate[2] += vMouseDelta[1]*0.01;
                glTranslatef(0.0, 0.0, g_vLandTranslate[2]); // translate w/ respect to z-axis
            }
            break;
        case ROTATE:
            if (g_iLeftMouseButton)
            {
                g_vLandRotate[0] += vMouseDelta[1]*0.01;
                g_vLandRotate[1] += vMouseDelta[0]*0.01;
                glRotatef(g_vLandRotate[0], 1.0, 0.0, 0.0); // rotate about x-axis
                glRotatef(g_vLandRotate[1], 0.0, 1.0, 0.0); // rotate about y-axis
            }
            if (g_iMiddleMouseButton)
            {
                g_vLandRotate[2] += vMouseDelta[1]*0.01;
                glRotatef(g_vLandRotate[2], 0.0, 0.0, 1.0); // rotate about z-axis
            }
            break;
    }
    g_vMousePos[0] = x;
    g_vMousePos[1] = y;
}

void mouseidle(int x, int y)
{
    g_vMousePos[0] = x;
    g_vMousePos[1] = y;
}

void mousebutton(int button, int state, int x, int y)
{
    
    switch (button)
    {
        case GLUT_LEFT_BUTTON:
            g_iLeftMouseButton = (state==GLUT_DOWN);
            break;
        case GLUT_MIDDLE_BUTTON:
            g_iMiddleMouseButton = (state==GLUT_DOWN);
            break;
        case GLUT_RIGHT_BUTTON:
            g_iRightMouseButton = (state==GLUT_DOWN);
            break;
    }

    switch(glutGetModifiers())
    {
        case GLUT_ACTIVE_CTRL:
            g_ControlState = TRANSLATE;
            break;
        case GLUT_ACTIVE_SHIFT:
            g_ControlState = SCALE;
            break;
        default:
            g_ControlState = ROTATE;
            break;
    }
    
    g_vMousePos[0] = x;
    g_vMousePos[1] = y;
}

void doIdle()
{
    /* make the screen update */
    glutPostRedisplay();
}

void keyboard(unsigned char c, int x, int y)
{
  if(c == 27)
  {
    exit(0);
  }
}

int main (int argc, char ** argv)
{
  if (argc<2)
  {  
  printf ("usage: %s <trackfile>\n", argv[0]);
  exit(0);
  }

  loadSplines(argv[1]);

  glutInit(&argc,argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH | GLUT_RGBA);
  glutInitWindowSize(600, 600);
  glutInitWindowPosition(0, 0);
  glutCreateWindow("Roller Coaster Simulation");
  
  init();
  glutDisplayFunc(display);
  glutIdleFunc(doIdle);
  glutKeyboardFunc(keyboard);
  glutMotionFunc(mousedrag);
  glutPassiveMotionFunc(mouseidle);
  glutMouseFunc(mousebutton);
  glutMainLoop();

  for(int i = 0; i < g_iNumOfSplines; ++i)
  {
    for(int j = 0; j + 2 < g_Splines[i].numControlPoints; ++j)
    {
      free(g_Splines[i].points);
    }
    free(g_Splines);
  }

  return 0;
}
