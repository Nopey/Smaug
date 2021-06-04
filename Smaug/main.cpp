#include "smaugapp.h"
#include "utils.h"
#include "log.h"
#include <GLFW/glfw3.h>

int main( int argc, char** argv )
{
	CommandLine::Set(argc, argv);

	glfwSetErrorCallback([]( int errorcode, const char *description ){
		Log::Warn( "GLFW ERROR[%d]: %s", errorcode, description );
	});

	return GetApp().run( argc, argv );
}
