#include <graphx.h>
#include <ti/getcsc.h>   // for key input
#include <ti/screen.h>   // screen dimensions
#include <cmath>
#include <tice.h>
#include <graphx.h>
#include <sys/lcd.h>

constexpr double PI = 3.14159265358979323846;


// Convert 0–1 floats to a 16-bit RGB565 value
uint16_t rgb565(int r, int g, int b) {
    uint8_t R = (uint8_t)std::round((float)r);
    uint8_t G = (uint8_t)std::round((float)g);
    uint8_t B = (uint8_t)std::round((float)b);

    return ((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3);
}   




void set_4bpp_mode() {
    // 1. Initialize graphics in the standard 8bpp mode first
    gfx_Begin();

    // 2. Access the LCD control register
    // Bits 1-3 set the BPP. 2 = 4bpp (16 colors)
    // We clear the current BPP bits and set them to 2.
    lcd_Control &= ~(0b111 << 1); 
    lcd_Control |= (2 << 1);
}

class ShaderProcess;
class obj;

struct pixel{ 
	uint8_t color;
	int depth;
};

struct fragment{
    int x;
    int y;
    int z; //depth
};

struct vertex{
    float x;
    float y;
    float z;
    int r;
    int g;
    int b;
};

struct poly{
    vertex v1;
    vertex v2;
    vertex v3;};

void matvecmult(const int mat[16], const float vec[3], vertex &result) {
    float x = vec[0];
    float y = vec[1];
    float z = vec[2];
    float w = 100; // matrix is scaled by 100

    float tx = (mat[0]*x + mat[1]*y + mat[2]*z + mat[3]*w) / 100.0f;
    float ty = (mat[4]*x + mat[5]*y + mat[6]*z + mat[7]*w) / 100.0f;
    float tz = (mat[8]*x + mat[9]*y + mat[10]*z + mat[11]*w) / 100.0f;
    float tw = (mat[12]*x + mat[13]*y + mat[14]*z + mat[15]*w) / 100.0f;

    if (tw != 0.0f) {
        result.x = tx / tw;
        result.y = ty / tw;
        result.z = tz / tw;
    } else {
        result.x = tx;
        result.y = ty;
        result.z = tz;
    }
}




void mat4mult(const int* a, const int* b, int* result) {
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            result[row * 4 + col] = 0.0f;
            for (int k = 0; k < 4; k++) {
                result[row * 4 + col] += a[row * 4 + k] * b[k * 4 + col];
            }
        }
    }
}

class obj{
public:
    obj(int numVertices, vertex* arr){
        int numPolys = numVertices / 3;
        polygons = new poly[numPolys];
        polyCount = numPolys;

        for (int i = 0; i < numPolys; i++){
            polygons[i] = {arr[i*3], arr[i*3+1], arr[i*3+2]};
        }
    }
    obj(){}
    int polyCount = 0;
    poly* polygons;
    float position[3] = {0, 0, -4};
    float rotation[3] = {0, 0, 0};
    float scale[3] = {3, 3, 1};

    void draw(pixel* pixelBuffer);

private:
    ShaderProcess* shaderProcess = nullptr;
};

void createTri(obj &b){
    vertex vertices[6] = {{-0.8, 0.8, 0.0, 255, 0, 0}, {0.8, -0.8, 0.0, 0, 255, 0}, {-0.8, -0.8, 0.0, 0, 0, 255},
                          {-0.8, 0.8, 0.0, 255, 0, 0}, {0.8,  0.8, 0.0, 0, 0, 255}, { 0.8, -0.8, 0.0, 0, 255, 0}};
    obj a(6, vertices);
    b = a;
}


class ShaderProcess{
public:
    ShaderProcess(pixel* screenbuffer, obj &object){
        buffer = screenbuffer;
        polygons = object.polygons;
        numPolygons = object.polyCount;
        GenPerspectiveMatrix(90.0f, ((float)320/240), 0.01f, 3.0f, projection);
    }
    void run(obj &object){
        
        GenModelMatrix(object.position, object.rotation, object.scale, model);
        
        mat4mult(projection, model, mp);
        
        VertexTransformation();
    }
    ShaderProcess(){}
    int model[16];
    int projection[16];
    int mp[16];
private:

    pixel* buffer = nullptr;
    poly* polygons;
    int numPolygons;
    void GenModelMatrix(float position[3], float rotation[3], float scale[3], int* M) {
        int tx = position[0]*100, ty = position[1]*100, tz = position[2]*100;
        int rx = rotation[0]*100 * PI / 180.0f;
        int ry = rotation[1]*100 * PI / 180.0f;
        int rz = rotation[2]*100 * PI / 180.0f;
        int sx = scale[0]*100, sy = scale[1]*100, sz = scale[2]*100;

        int cx = cos(rx)*100, sx_sin = sin(rx)*100;
        int cy = cos(ry)*100, sy_sin = sin(ry)*100;
        int cz = cos(rz)*100, sz_sin = sin(rz)*100;

        // Rotation matrices
        int Rx[16] = {
            1,     0,      0, 0,
            0,    cx,   -sx_sin, 0,
            0, sx_sin,    cx, 0,
            0,     0,      0, 1
        };
        int Ry[16] = {
            cy, 0, sy_sin, 0,
            0,  1,     0, 0,
           -sy_sin, 0, cy, 0,
            0,  0,     0, 1
        };
        int Rz[16] = {
            cz, -sz_sin, 0, 0,
            sz_sin,  cz, 0, 0,
            0,       0, 1, 0,
            0,       0, 0, 1
        };
        int S[16] = {
            sx, 0,  0, 0,
            0, sy,  0, 0,
            0,  0, sz, 0,
            0,  0,  0, 1
        };
        int T[16] = {
            1, 0, 0, tx,
            0, 1, 0, ty,
            0, 0, 1, tz,
            0, 0, 0, 1
        };

        int Rxy[16], R[16], RS[16], TRS[16];
        mat4mult(Ry, Rx, Rxy);
        mat4mult(Rz, Rxy, R);
        mat4mult(R, S, RS);
        mat4mult(T, RS, TRS);

        for (int i = 0; i < 16; i++) M[i] = TRS[i];  
    }
    void GenPerspectiveMatrix(float fovY, float aspect, float zNear, float zFar, int* M){
        float f = 1.0f / tan(fovY * 0.5f * PI / 180.0f);

        M[0] = f*100 / aspect; M[1] = 0;     M[2] = 0;                                M[3] = 0;
        M[4] = 0;              M[5] = f*100; M[6] = 0;                                M[7] = 0;
        M[8] = 0;              M[9] = 0;     M[10] = (zFar+zNear)/(zNear-zFar)*100;   M[11] = (2*zFar*zNear)/(zNear-zFar)*100;
        M[12] = 0;             M[13] = 0;    M[14] = -100;                          M[15] = 0;
    }

    void VertexTransformation() {
    vertex v1, v2, v3;
    float vec[3];
    for (int i = 0; i < numPolygons; i++) {
        // --- v1 ---
        vec[0] = polygons[i].v1.x;
        vec[1] = polygons[i].v1.y;
        vec[2] = polygons[i].v1.z;
        matvecmult(mp, vec, v1);
        v1.r = polygons[i].v1.r;
        v1.g = polygons[i].v1.g;
        v1.b = polygons[i].v1.b;

        // --- v2 ---
        vec[0] = polygons[i].v2.x;
        vec[1] = polygons[i].v2.y;
        vec[2] = polygons[i].v2.z;
        matvecmult(mp, vec, v2);
        v2.r = polygons[i].v2.r;
        v2.g = polygons[i].v2.g;
        v2.b = polygons[i].v2.b;

        // --- v3 ---
        vec[0] = polygons[i].v3.x;
        vec[1] = polygons[i].v3.y;
        vec[2] = polygons[i].v3.z;
        matvecmult(mp, vec, v3);
        v3.r = polygons[i].v3.r;
        v3.g = polygons[i].v3.g;
        v3.b = polygons[i].v3.b;

        // send to rasterizer
        Rasterize(v1, v2, v3, buffer);
    }
}

    void Rasterize(vertex v1, vertex v2, vertex v3, pixel* buffer){
        int p1[3] = {(int)((v1.x/2 + 0.5f)*79), (int)((v1.y/-2 + 0.5f)*59), (int)(v1.z*100)};
        int p2[3] = {(int)((v2.x/2 + 0.5f)*79), (int)((v2.y/-2 + 0.5f)*59), (int)(v2.z*100)};
        int p3[3] = {(int)((v3.x/2 + 0.5f)*79), (int)((v3.y/-2 + 0.5f)*59), (int)(v3.z*100)};

        int minX = (int)p1[0]; if ((int)p2[0] < minX) minX = (int)p2[0]; if ((int)p3[0] < minX) minX = (int)p3[0]; if (minX < 0) minX = 0;
        int maxX = (int)p1[0]; if ((int)p2[0] > maxX) maxX = (int)p2[0]; if ((int)p3[0] > maxX) maxX = (int)p3[0]; if (maxX > 79) maxX = 79;
        int minY = (int)p1[1]; if ((int)p2[1] < minY) minY = (int)p2[1]; if ((int)p3[1] < minY) minY = (int)p3[1]; if (minY < 0) minY = 0;
        int maxY = (int)p1[1]; if ((int)p2[1] > maxY) maxY = (int)p2[1]; if ((int)p3[1] > maxY) maxY = (int)p3[1]; if (maxY > 59) maxY = 59;

        int vecAB[2] = {p2[0]-p1[0], p2[1]-p1[1]};
        int vecBC[2] = {p3[0]-p2[0], p3[1]-p2[1]};
        int vecCA[2] = {p1[0]-p3[0], p1[1]-p3[1]};

        int test1;
        int test2;
        int test3;

        int vecToPixel[2];

        int weightA, weightB, weightC;
        int areaA, areaB, areaC;

        int areaTotal = abs( (p2[0] - p1[0])*(p3[1] - p1[1]) - (p2[1] - p1[1])*(p3[0] - p1[0]) );
        int depth, r, g, b;
        for (int j = minY; j <= maxY; j++) {
            for (int i = minX; i <= maxX; i++) {

                vecToPixel[0] = i-p1[0];
                vecToPixel[1] = j-p1[1];
                test1 = vecAB[0]*vecToPixel[1] - vecAB[1]*vecToPixel[0];

                vecToPixel[0] = i-p2[0];
                vecToPixel[1] = j-p2[1];
                test2 = vecBC[0]*vecToPixel[1] - vecBC[1]*vecToPixel[0];
                
                vecToPixel[0] = i-p3[0];
                vecToPixel[1] = j-p3[1];
                test3 = vecCA[0]*vecToPixel[1] - vecCA[1]*vecToPixel[0];



                if ((test1 >= 0 && test2 >= 0 && test3 >= 0) || 
                    (test1 <= 0 && test2 <= 0 && test3 <= 0)){
                    
                    areaA = abs( (p2[0] - i)*(p3[1] - j) - (p2[1] - j)*(p3[0] - i) );
                    areaB = abs( (p3[0] - i)*(p1[1] - j) - (p3[1] - j)*(p1[0] - i) );
                    areaC = abs( (p1[0] - i)*(p2[1] - j) - (p1[1] - j)*(p2[0] - i) );
                    weightA = (areaA*1000) / areaTotal;
                    weightB = (areaB*1000) / areaTotal;
                    weightC = (areaC*1000) / areaTotal;

                    depth = (weightA*p1[2] + weightB*p2[2] + weightC*p3[2])/1000;

                    //r = (weightA*v1.r + weightB*v2.r + weightC*v3.r);
                    //g = (weightA*v1.g + weightB*v2.g + weightC*v3.g);
                    //b = (weightA*v1.b + weightB*v2.b + weightC*v3.b);
                    if (buffer[i + 80*j].depth < depth){
                        //buffer[i + 80*j] = {rgb565(r,g,b), depth};
                        buffer[i + 80*j] = {5, depth};
                    }
                    
                    
                }
            }
        }
        
        
    }
    void Shade(){}
    void DepthTest(){}
};

void obj::draw(pixel* pixelBuffer){
    if (shaderProcess == nullptr){
        shaderProcess = new ShaderProcess(pixelBuffer, *this);
    }
    shaderProcess->run(*this);
}


class World{
public:
    World(){
        createTri(a);
    }
    obj a;
private:
    obj objects[10];};

void drawBuffer(pixel* buffer) {

    uint8_t (*vram)[320] = gfx_vbuffer;


    for (int y = 0; y < 60; y++) {
        for (int x = 0; x < 80; x += 1) {
            uint8_t color = (buffer[x + y*80].color & 0x0F) << 4
              | (buffer[x + y*80].color & 0x0F);

            if (buffer[x + y*80].depth > -99999){
                int screenY = y * 2;
                int screenX = x * 2;
                vram[screenY][screenX] = color;
                vram[screenY][screenX+1] = color;
                

                vram[screenY+1][screenX] = color;
                vram[screenY+1][screenX+1] = color;
                

                vram[screenY+2][screenX] = color;
                vram[screenY+2][screenX+1] = color;
                

                vram[screenY+3][screenX] = color;
                vram[screenY+3][screenX+1] = color;
                
                vram[screenY+4][screenX] = color;
                vram[screenY+4][screenX+1] = color;

                vram[screenY+5][screenX] = color;
                vram[screenY+5][screenX+1] = color;

            }

        }
    }
}

void updateScreen(pixel* buffer, World world){
    gfx_SetDrawBuffer();   // switch drawing to the offscreen buffer
    gfx_FillScreen(1);
    for (int i = 0; i < 80 * 60; i++) {
        buffer[i].color = 1; // or whatever your background color is
        buffer[i].depth = -99999;
    }
    world.a.draw(buffer);
    drawBuffer(buffer);
    gfx_SwapDraw();
}

int main(void) {
	pixel* buffer = new pixel[80*60];

    // Initialize the graphics mode
    gfx_Begin();
    World world;
    set_4bpp_mode();

    // Wait for a key press
    int b = 0;
    while (!os_GetCSC()){
    	updateScreen(buffer, world);
        world.a.rotation[1] += 5;
    }
    // End graphics mode
    gfx_End();

    return 0;
}
