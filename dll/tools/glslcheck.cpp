// Offline shader check: compiles PA3aiD's iso shaders on this PC's GL driver (hidden window), prints the info logs.
#include <windows.h>
#include <GL/gl.h>
#include <cstdio>
#include <string>
#include "shaders.h"
typedef char GLchar;
typedef GLuint (APIENTRY* PCS)(GLenum); typedef void (APIENTRY* PSS)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void (APIENTRY* PCO)(GLuint); typedef void (APIENTRY* PGI)(GLuint, GLenum, GLint*); typedef void (APIENTRY* PGL)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLuint (APIENTRY* PCP)(void); typedef void (APIENTRY* PAS)(GLuint, GLuint); typedef void (APIENTRY* PLP)(GLuint);
static int Compile(GLenum type, const char* src, const char* name, GLuint& out) {
    PCS cs = (PCS)wglGetProcAddress("glCreateShader"); PSS ss = (PSS)wglGetProcAddress("glShaderSource"); PCO co = (PCO)wglGetProcAddress("glCompileShader");
    PGI gi = (PGI)wglGetProcAddress("glGetShaderiv"); PGL gl = (PGL)wglGetProcAddress("glGetShaderInfoLog");
    out = cs(type); ss(out, 1, &src, nullptr); co(out); GLint ok = 0; gi(out, 0x8B81, &ok);
    char log[8192] = {0}; gl(out, sizeof log, nullptr, log);
    printf("%s: %s\n%s\n", name, ok ? "COMPILED" : "FAILED", log); return ok;
}
static std::string Slurp(const char* f) { FILE* p = fopen(f, "rb"); std::string s; if (!p) return s; char b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, p)) > 0) s.append(b, n); fclose(p); return s; }
int main(int argc, char** argv) {
    HWND w = CreateWindowA("STATIC", "glslcheck", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, 0, 0, 0, 0); HDC dc = GetDC(w);
    PIXELFORMATDESCRIPTOR pfd = { sizeof pfd, 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
    SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd); HGLRC rc = wglCreateContext(dc); wglMakeCurrent(dc, rc);
    printf("GL %s / %s\n", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));
    std::string sv = argc > 2 ? Slurp(argv[1]) : std::string(kIsoVS), sf = argc > 2 ? Slurp(argv[2]) : std::string(kIsoFS);
    GLuint vs, fs; int a = Compile(0x8B31, sv.c_str(), argc > 2 ? argv[1] : "kIsoVS", vs), b = Compile(0x8B30, sf.c_str(), argc > 2 ? argv[2] : "kIsoFS", fs);
    int ok = a && b;
    if (ok) { PCP cp = (PCP)wglGetProcAddress("glCreateProgram"); PAS as = (PAS)wglGetProcAddress("glAttachShader"); PLP lp = (PLP)wglGetProcAddress("glLinkProgram");
        PGI gp = (PGI)wglGetProcAddress("glGetProgramiv"); PGL gl = (PGL)wglGetProcAddress("glGetProgramInfoLog");
        GLuint p = cp(); as(p, vs); as(p, fs); lp(p); GLint l = 0; gp(p, 0x8B82, &l); char log[8192] = {0}; gl(p, sizeof log, nullptr, log);
        printf("link: %s\n%s\n", l ? "LINKED" : "FAILED", log); ok = l; }
    return ok ? 0 : 1;
}
