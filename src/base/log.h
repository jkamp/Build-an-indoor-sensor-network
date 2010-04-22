#ifndef _TEAMLK_LOG_H_
#define _TEAMLK_LOG_H_

#ifdef TEAMLK_DEBUG

#include <stdio.h>
#include <stdlib.h>

#define LOG(...) printf(__VA_ARGS__)

#define TRACE(...) \
	do { \
		printf("[%s %d] ", __FILE__, __LINE__); \
		printf(__VA_ARGS__); \
	}while(0)

#define ASSERT(x) \
	do { \
		if(!(x)){ \
	   		LOG("ASSERT FAILED @ %s %d\n", __FILE__, __LINE__);  \
			exit(-1); \
		} \
	}while(0);
#else /*TEAMLK_DEBUG*/
#define LOG(...)
#define TRACE(...)
#define ASSERT(...)
#endif /*TEAMLK_DEBUG*/

#endif /*_TEAMLK_LOG_H_ */
