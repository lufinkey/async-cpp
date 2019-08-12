//
//  Macros.hpp
//  AsyncCpp
//
//  Created by Luis Finke on 8/11/19.
//  Copyright © 2019 Luis Finke. All rights reserved.
//

#pragma once

#include <exception>
#include <iostream>

namespace fgl {
	#ifndef FGL_ASSERT
		#define FGL_ASSERT(condition, message) { \
			if(!(condition)) { \
				std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
					<< " line " << __LINE__ << ": " << (message) << std::endl; \
				std::terminate(); \
			} \
		}
	#endif
	
	#ifndef FGL_WARN
		#ifdef FGL_SHOW_WARNINGS
			#define FGL_WARN(message) { \
				std::cerr << "Warning: " << (message) << std::endl; \
			}
			#else
				#define FGL_WARN(message)
			#endif
	#endif
}
