#ifndef VIDEO
#define VIDEO
#include "video.h"
#endif // !VIDEO

#ifndef OPENCV
#define OPENCV
#include <opencv2\opencv.hpp> 
using namespace cv;
#endif // !OPENCV

#include <iostream>
#include <iomanip>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <algorithm>

using namespace std;
using namespace cv;

#define sqr(_x) ((_x) * (_x))

const double pi = 3.141592653586369;
const double small_number = 1e-10;
const double eps = 1e-8;
const int route[8][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 }, { 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 } };

int main()
{
	return 0;
}