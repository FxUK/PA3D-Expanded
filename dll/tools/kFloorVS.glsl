
varying vec2 wp;
void main(void) { wp = gl_Vertex.xy; gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex; }
