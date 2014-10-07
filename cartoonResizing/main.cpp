/*
*	Copyright (C)   Lyq root#lyq.me
*	File Name     : cartoonResizing
*	Creation Time : 2014-10-2 23:26:06
*	Environment   : Windows8.1-64bit VS2013 OpenCV2.4.9
*	Homepage      : http://www.lyq.me
*/

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

#define INF 2000000000
#define sqr(_x) ((_x) * (_x))

void help(void);
void printMatDouble(char*, const Mat&);
bool openCapture(VideoCapture&, const char*);
void backgroundSegmentation(VideoCapture&, Mat&);
void divImg(const Mat&, Mat&, Mat&, Mat&, Mat&);
void calcEnergy(const Mat&, const Mat&, const Mat&, Mat&, bool, bool, double, double, double);
void erosionFrame(const Mat&, Mat&, Mat&, Mat&, const Mat&, int);
void expansionFrame(const Mat&, Mat&, Mat&, const Mat&, int);
void calcHistory(Mat&, const Mat&, double);
void seamCarving(VideoCapture&, Mat&, double, double, double, double, double);

int main(void)
{
	char *filename = "shot1_720p.avi";
	VideoCapture cap;
	Mat bg_model;
	const double w_bg = 0.9;
	const double w_bond = 0.05;
	const double w_preserve = 0.6;
	const double w_history = 0.8;
	const double resizingRate = 0.2;

	help();

	int state = openCapture(cap, filename);
	if (!state) return -2;

	backgroundSegmentation(cap, bg_model);
	seamCarving(cap, bg_model, w_bg, w_bond, w_preserve, w_history, resizingRate);
	
	return 0;
}

void help(void)
{
	printf(	"===	Copyright (C)   Lyq root#lyq.me\n"
			"===	File Name : cartoonResizing\n"
			"===	Creation Time : 2014-10-2 23:26:06 UTC+8\n"
			"===	Environment : Windows8.1-64bit VS2013 OpenCV2.4.9\n"
			"===	Homepage : http ://www.lyq.me\n"
			"===	\n"
			"===	This program demostrated a simple method of cartoon resizing.\n"
			"===	It learns the background and calculates each pixel's Energy.\n"
			"===	Reference: Mixture of Gaussian and Seam Carving\n");
}
void printMatDouble(char *filename, const Mat &mat)
{
	bool flag = false;
	FILE *file;
	if (filename != NULL) 
	{
		flag = true;
		file = fopen(filename, "w");
	}
	
	for (int y = 0; y < mat.rows; y++)
	{
		if (flag) fprintf(file, "[");
			else printf("[");
		for (int x = 0; x < mat.cols; x++)
		{
			//cout << y << " " << x << endl;
			if (flag) fprintf(file, "%5.1lf", mat.ptr<double>(y)[x]);
				else printf("%5.1lf", mat.ptr<double>(y)[x]);
			if (x != mat.cols - 1)
				if (flag) fprintf(file, ""); 
					else printf("");
			else
				if (flag) fprintf(file, "]\n");
					else printf("]\n");
		}
	}
	if (flag)
	{
		fprintf(file, "\n\n");
		fclose(file);
	}else printf("\n\n");
}
bool openCapture(VideoCapture &cap, const char *filename)
{
	if (!cap.open(filename))
	{
		cout << "The filename error!" << endl;
		return false;
	}
	return true;
}
void backgroundSegmentation(VideoCapture &cap, Mat &bg_model)
{
	Mat frame, in_frame, fg_mask, out_frame, temp_model;
	BackgroundSubtractorMOG bg_subtractor;

	//namedWindow("video", WINDOW_NORMAL);
	//namedWindow("foreground", WINDOW_NORMAL);
	bg_subtractor.set("noiseSigma", 20);
	temp_model.create(Size((int)cap.get(CV_CAP_PROP_FRAME_WIDTH), (int)cap.get(CV_CAP_PROP_FRAME_HEIGHT)), CV_64FC1);
	temp_model = Scalar::all(0);

	while (cap.read(frame))
	{
		cvtColor(frame, in_frame, COLOR_RGB2GRAY);
		bg_subtractor(in_frame, fg_mask, -1);
		//out_frame = Scalar::all(0);
		//in_frame.copyTo(out_frame, fg_mask);
	
		add(fg_mask, temp_model, temp_model, noArray(), CV_64FC1);
		
		//imshow("video", in_frame);
		//imshow("foreground", out_frame);
		//waitKey(1);
	}

	int frame_count = (int)cap.get(CV_CAP_PROP_FRAME_COUNT);
	bg_model = temp_model.mul(1.0 / (double)(255*frame_count));
	//sqrt(bg_model, bg_model); // make a scale bg_model = bg_model ^ (1/2)
	bg_model = bg_model.mul(3.5);
	
	//printMatDouble("bg_model.txt", bg_model);
}
void divImg(const Mat &frame, Mat &bond_x, Mat &bond_y, Mat &grad_x, Mat &grad_y)
{
	Mat canny_frame, temp_mat;
	Sobel(frame, temp_mat, CV_16SC1, 1, 0, 1, 0.5);
	Mat(abs(temp_mat)).convertTo(grad_x, CV_64FC1);
	grad_x = grad_x.mul(1.0 / 255);
	Sobel(frame, temp_mat, CV_16SC1, 0, 1, 1, 0.5);
	Mat(abs(temp_mat)).convertTo(grad_y, CV_64FC1);
	grad_y = grad_y.mul(1.0 / 255);

	GaussianBlur(frame, canny_frame, Size(7, 7), 1.5, 1.5);
	Canny(canny_frame, canny_frame, 50, 100, 3, true);
	Sobel(canny_frame, temp_mat, CV_16SC1, 1, 0, 1, 0.5);
	Mat(abs(temp_mat)).convertTo(bond_x, CV_64FC1);
	bond_x = bond_x.mul(1.0 / 255);
	Sobel(canny_frame, temp_mat, CV_16SC1, 0, 1, 1, 0.5);
	Mat(abs(temp_mat)).convertTo(bond_y, CV_64FC1);
	bond_y = bond_y.mul(1.0 / 255);
}
void calcEnergy(const Mat &frame, const Mat &bg_model, const Mat &history, Mat &energy, bool direct_x, bool direct_y, double w_bg, double w_bond, double w_history)
{
	if (!(direct_x ^ direct_y))
	{
		cout << "ERROR: in calcEnergy, direct_x & direct_y are same." << endl;
		return; 
	}

	Mat bond_x, bond_y, grad_x, grad_y;

	divImg(frame, bond_x, bond_y, grad_x, grad_y);

	if (direct_x)
	{
		Mat temp_mat;
		//sqrt(grad_x.mul(grad_x) + grad_y.mul(grad_y), temp_mat);
		temp_mat = max(grad_x, grad_y);
		printMatDouble("temp_mat.txt", temp_mat);
		energy = bg_model.mul(w_bg) + bond_x.mul(w_bond) + temp_mat.mul(1-w_bg-w_bond);
		//sqrt(history, temp_mat);
		printMatDouble("history.txt", temp_mat);
		//multiply(energy, temp_mat, energy);
		return;
	}
	if (direct_y)
	{
		Mat temp_mat;
		sqrt(grad_x.mul(grad_x) + grad_y.mul(grad_y), temp_mat);
		energy = bg_model.mul(w_bg) + bond_y.mul(w_bond) + temp_mat.mul(1-w_bg-w_bond);
		sqrt(history, temp_mat);
		multiply(energy, temp_mat, energy);
		return;
	}
}
void erosionFrame(const Mat &in_frame, Mat &cut_frame, Mat &mask_frame, Mat &out_frame, const Mat &energy, int num)
{
	
	static double dp[2][800], val[600][800];
	static short dp_last[600][800], dp_pa[2][800], cur;
	static bool exist[800][800];

	mask_frame.create(in_frame.size(), in_frame.type());
	out_frame.create(Size(in_frame.cols - num, in_frame.rows), in_frame.type());

	for (int y = 0; y < in_frame.rows; y++)
	{
		for (int x = 0; x < in_frame.cols; x++) val[y][x] = energy.ptr<double>(y)[x];
	}
	for (int y = 0; y < in_frame.rows; y++)
	{
		for (int x = 0; x < in_frame.cols; x++) exist[y][x] = true;
	}
	for (int x = 0; x < in_frame.cols; x++)	dp_last[0][x] = -1;
	
	while (num > 0)
	{
		for (int x = 0; x < in_frame.cols; x++)	dp[0][x] = val[0][x];
		for (int x = 0; x < in_frame.cols; x++) dp_pa[0][x] = x;
		cur = 0;

		for (int y = 1; y < in_frame.rows; y++)
		{
			double add1, add2, add3;

			for (int x = 0; x < in_frame.cols; x++)
			{
				if (!exist[y][x]) continue;

				if (x != 0 && exist[y-1][x-1]) add1 = dp[cur][x-1];
					else add1 = INF;
				if (exist[y-1][x]) add2 = dp[cur][x];
					else add2 = INF;
				if (x != (in_frame.cols-1) && exist[y-1][x+1]) add3 = dp[cur][x+1];
					else add3 = INF;

				if (add1 <= min(add2,add3) && add1 < INF)
				{
					dp_last[y][x] = x - 1;
					dp_pa[1-cur][x] = dp_pa[cur][x-1];
					dp[1-cur][x] = add1 + val[y][x];
				}else if (add2 <= min(add1,add3) && add2 < INF)
				{
					dp_last[y][x] = x;
					dp_pa[1-cur][x] = dp_pa[cur][x];
					dp[1-cur][x] = add2 + val[y][x];
				}else if (add3 <= min(add1,add2) && add3 < INF)
				{
					dp_last[y][x] = x + 1;
					dp_pa[1-cur][x] = dp_pa[cur][x+1];
					dp[1-cur][x] = add3 + val[y][x];
				}else
				{
					dp_last[y][x] = -1;
					dp[1-cur][x] = INF;
				}
			}
			cur = 1 - cur;
		}
		
		int last_pa = -1;
		int last_x = -1;
		double temp = INF;
		for (int x = 0; x < in_frame.cols; x++)
		{
			if (!exist[in_frame.rows-1][x]) continue;
			if (dp_pa[cur][x] != last_pa)
			{
				if (temp < INF)
				{
					if (num > 0) num--;
						else break;
					int last_y = in_frame.rows - 1;
					while (last_x != -1)
					{
						exist[last_y][last_x] = false;
						last_x = dp_last[last_y][last_x];
						last_y--;
					}
				}
				last_pa = dp_pa[cur][x];
				last_x = x;
				temp = dp[cur][x];
			}else if (temp > dp[cur][x])
			{
				temp = dp[cur][x];
				last_x = x;
			}
		}
		if (temp < INF)
		{
			if (num > 0) num--;
				else break;
			int last_y = in_frame.rows - 1;
			while (last_x != -1)
			{
				exist[last_y][last_x] = false;
				last_x = dp_last[last_y][last_x];
				last_y--;
			}
		}
	}

	static short row_tail[600];
	std::memset(row_tail, 0, sizeof(row_tail));
	
	for (int y = 0; y < in_frame.rows; y++)
	{
		for (int x = 0; x < in_frame.cols; x++)
		{
			mask_frame.ptr<uchar>(y)[x] = (int)(exist[y][x]) * 255;
			if (exist[y][x])
			{
				out_frame.ptr<uchar>(y)[row_tail[y]] = in_frame.ptr<uchar>(y)[x];
				row_tail[y]++;
			}
		}
	}
	in_frame.copyTo(cut_frame, mask_frame);
}
void expansionFrame(const Mat &in_frame, Mat &cut_frame, Mat &out_frame, const Mat &energy, int num)
{

}
void calcHistory(Mat &history, const Mat &mask, double w_preserve)
{ 
	Mat temp_mat;
	mask.convertTo(temp_mat, CV_64FC1);
	temp_mat = temp_mat.mul(1.0 / 255);
	history = history.mul(w_preserve) + temp_mat.mul(1-w_preserve);
}
void seamCarving(VideoCapture &cap, Mat &bg_model, double w_bg, double w_bond, double w_preserve, double w_history, double rate)
{
	int num_width, num_height, count;
	char filename[40];
	Mat frame, in_frame, energy, temp_frame, out_frame, cut_frame, mask_frame, history_direct_x, history_direct_y;
	Size size;
	Scalar bg_model_ave;
	
	bg_model_ave= sum(bg_model);
	bg_model_ave.val[0] /= (bg_model.rows*bg_model.cols);
	bg_model_ave.val[0] *= 2;
	cout << bg_model_ave.val[0];
	size = Size((int)cap.get(CV_CAP_PROP_FRAME_WIDTH), (int)cap.get(CV_CAP_PROP_FRAME_HEIGHT));
	num_width = (int)(size.width * rate + 0.5);
	num_height = (int)(num_width * bg_model_ave.val[0] + 0.5);
	num_width -= num_height;
	history_direct_x = Mat::ones(size, CV_64FC1);
	history_direct_y = Mat::ones(Size(size.width-num_width, size.height), CV_64FC1);

	cap.set(CV_CAP_PROP_POS_AVI_RATIO, 0);
	
	count = 0;
  	while (cap.read(frame))
	{
		cvtColor(frame, in_frame, COLOR_RGB2GRAY);
		
		calcEnergy(in_frame, bg_model, history_direct_x, energy, true, false, w_bg, w_bond, w_history);
		//cv::imshow("energy", energy);
		//cv::imshow("bg", bg_model);
		//cv::waitKey(0);
		erosionFrame(in_frame, cut_frame, mask_frame, temp_frame, energy, num_width);
		calcHistory(history_direct_x, mask_frame, w_preserve);
		//cv::imshow("cut_frame", cut_frame);  
		//cv::imshow("out_frame", temp_frame);
		//cv::imshow("history_direct_x", history_direct_x);
		//cv::imshow("energy", energy);
		sprintf(filename, "cut_frame/%d.bmp", count);
		imwrite(filename, cut_frame);
		sprintf(filename, "out_frame/%d.bmp", count);
		imwrite(filename, temp_frame);
		sprintf(filename, "history_direct_x/%d.bmp", count);
		imwrite(filename, history_direct_x);
		sprintf(filename, "energy/%d.bmp", count);
		imwrite(filename, energy);
		printMatDouble("energy.txt", energy);
		//cv::waitKey(0);
		//expansionFrame(temp_frame, cut_frame, out_frame, energy, num_height);
		
		//imshow("energy", energy);
		//waitKey(1);

		count++; 
	}
}
