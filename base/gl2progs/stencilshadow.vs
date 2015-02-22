#version 120     

varying vec4            	var_Color;
  
uniform vec4 				u_lightOrigin;

void main( void ) {
	if( gl_Vertex.w == 1.0 ) {
		// mvp transform into clip space     
		gl_Position = ftransform(); 
	} else {
		// project vertex position to infinity
		vec4 vertex = gl_Vertex - u_lightOrigin;
		gl_Position = gl_ModelViewProjectionMatrix * vertex;
	}

	// primary color
	var_Color = gl_Color; 
}