#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "pgm.h"

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#define MAX_SOURCE_SIZE (0x100000) //1MB

#define DEBUG 1
#define USE_GPU 1
#define PARALLEL 1
#define KERNEL_FILE_NAME "NegativeKernel.cl"






#if USE_GPU
	#define CL_DEVICE_TO_USE CL_DEVICE_TYPE_GPU
#else
	#define CL_DEVICE_TO_USE CL_DEVICE_TYPE_CPU
#endif

struct options
{
	int filter_size_flag; //-f option
	int filter_size;

	int image_input_flag; //-i option
	int image_input;

	int image_output_flag; //-o option
	int image_output;

}*options_args;

void help();

//GLOBAL VARIABLES

//OPENCL
cl_device_id device_id = NULL;
cl_context context = NULL;
cl_command_queue command_queue = NULL;
cl_program program = NULL;
cl_kernel kernel = NULL;
cl_platform_id platform_id = NULL;
cl_uint ret_num_devices;
cl_uint ret_num_platforms;
cl_int ret;

//KERNEL FILE
char* kernel_str;
size_t kernel_size;

//IMAGE
unsigned char* img = NULL;
int width;
int height;

//MEMORY BUFFERS
cl_mem inputImage;
cl_mem outputImage;
unsigned char* imgOutput;


void check_error(char* message,int error_code)
{
	#if DEBUG
		if (error_code == CL_SUCCESS)
		{
			fprintf(stdout, "DEBUG: %s OK %s", message, "\n");
		}
		else
		{
			fprintf(stderr, "ERROR: %s %d %s", message, error_code,"\n");
			exit(error_code);
		}
	#endif
}


/**
 * process the command line arguments
 */
void process_args(int argc, char** argv)
{
	if (argc == 1)
	{
		help();
		exit(1);
	}

	char *optString = "-f:-i:-o:";

	options_args = (struct options*) malloc(sizeof(struct options));
	int opt = getopt(argc, argv, optString);
	while (opt != -1)
	{
		switch (opt)
		{
		case 'f':
			options_args->filter_size_flag = 1;
			options_args->filter_size = atoi(optarg);
			break;
		case 'i':
			options_args->image_input_flag = 1;
			options_args->image_input = atoi(optarg);
			break;
		case 'o':
			options_args->image_output_flag = 1;
			options_args->image_output = atoi(optarg);
			break;

		default:
			break;
		}
		opt = getopt(argc, argv, optString);
	}

	//if all args OK
	if (options_args->image_input_flag && options_args->filter_size_flag && options_args->image_output_flag)
	{
		fprintf(stdout,"Arguments ok!");
	}
	else
	{
		fprintf(stderr,"Arguments error!");
		help();
		exit(1);
	}

}

void help()
{
	printf("Help: OpenCL_Blur\n");
	printf("-i image input\n");
	printf("-o image output\n");
	printf("-f filter size\n");
}

void init_opencl()
{
	ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
	check_error("clGetPlatformIDs... ",ret);
	ret = clGetDeviceIDs(platform_id, CL_DEVICE_TO_USE, 1, &device_id, &ret_num_devices);
	check_error("clGetDeviceIDs... ",ret);
	context = clCreateContext( NULL, 1, &device_id, NULL, NULL, &ret);
	check_error("clCreateContext... ",ret);
	command_queue = clCreateCommandQueue(context, device_id, 0, &ret);
	check_error("clCreateCommandQueue... ",ret);
}

void read_kernel_file()
{
	FILE *fp;
	fp = fopen(KERNEL_FILE_NAME, "r");
	check_error("Reading kernel file... ",fp == NULL);
	kernel_str = (char*) malloc(MAX_SOURCE_SIZE);
	check_error("Malloc kernel file... ",kernel_str == NULL);
	kernel_size = fread(kernel_str, 1, MAX_SOURCE_SIZE, fp);
	fclose(fp);
}

void read_image_file()
{
	int status = pgm_load(&img, &height, &width, "images\\Lena-512x512.pgm");
	check_error("Reading image file... ",status);
}

void build_kernel()
{
	program = clCreateProgramWithSource(context, 1, (const char **) &kernel_str, (const size_t *) &kernel_size, &ret);
	check_error("clCreateProgramWithSource... ",ret);
	ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
	check_error("clBuildProgram... ",ret);
	kernel = clCreateKernel(program, "blur", &ret);
	check_error("clCreateKernel... ",ret);
}

void create_and_fill_memory_buffers()
{
	inputImage = clCreateBuffer(context, CL_MEM_READ_ONLY, width * height * sizeof(unsigned char), NULL, &ret);
	check_error("Creating input image buffer... ",ret);
	ret = clEnqueueWriteBuffer(command_queue, inputImage, CL_TRUE, 0, 262144, img, 0, NULL, NULL);
	check_error("Writing input image to GPU... ",ret);
	outputImage = clCreateBuffer(context, CL_MEM_WRITE_ONLY, width * height * sizeof(unsigned char), NULL, &ret);
	check_error("Creating output image buffer... ",ret);
}

void set_kernel_args()
{

	//cl_mem blurFilter = inputImage;
	//cl_mem filterSize = NULL;

	ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *) &inputImage);
	check_error("clSetKernelArg input_image... ",ret);
	ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *) &inputImage);
	check_error("clSetKernelArg blur_filter... ",ret);
	ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *) &outputImage);
	check_error("clSetKernelArg output_image... ",ret);
	/*
	 ret = clSetKernelArg(kernel, 3, sizeof(cl_mem), (void *)&width);
	 if (ret == CL_SUCCESS)
	 {
	 printDebug("clSetKernelArg 3 OK");
	 }
	 else
	 {
	 printError("clSetKernelArg 3 ERROR");
	 exit(1);
	 }
	 ret = clSetKernelArg(kernel, 4, sizeof(cl_mem), (void *)&height);
	 if (ret == CL_SUCCESS)
	 {
	 printDebug("clSetKernelArg 4 OK");
	 }
	 else
	 {
	 printError("clSetKernelArg 4 ERROR");
	 exit(1);
	 }
	 ret = clSetKernelArg(kernel, 5, sizeof(cl_mem), (void *)&filterSize);
	 if (ret == CL_SUCCESS)
	 {
	 printDebug("clSetKernelArg 4 OK");
	 }
	 else
	 {
	 printError("clSetKernelArg 4 ERROR");
	 exit(1);
	 }
	 */
}

void start_kernel()
{
	const size_t global_size[] = { width, height };
	//const size_t  local_size = {16,16};
	ret = clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, global_size,NULL, 0, NULL, NULL);
	//ret = clEnqueueTask(command_queue, kernel, 0, NULL,NULL);
	check_error("clEnqueueNDRangeKernel... ",ret);
}

void read_output_image()
{
	imgOutput = malloc(width * height * sizeof(unsigned char));
	check_error("Malloc image output from GPU... ",ret);
	ret = clEnqueueReadBuffer(command_queue, outputImage, CL_TRUE, 0, width * height * sizeof(char), imgOutput, 0, NULL, NULL);
	check_error("clEnqueueReadBuffer... ",ret);
}

void save_image()
{
	//REGION save the image
	int status = pgm_save(imgOutput, height, width, "images_output\\Lena-512x512_BLURRED.pgm");
	check_error("Save image to file... ",ret);
}



void free_resources()
{
	ret = clFlush(command_queue);
	ret = clFinish(command_queue);
	ret = clReleaseKernel(kernel);
	ret = clReleaseProgram(program);
	ret = clReleaseMemObject(inputImage);
	ret = clReleaseMemObject(outputImage);
	ret = clReleaseCommandQueue(command_queue);
	ret = clReleaseContext(context);
	free(kernel_str);
	free(imgOutput);
}

int main(int argc, char** argv)
{
	process_args(argc, argv);
	init_opencl();
	read_kernel_file();
	read_image_file();
	build_kernel();
	create_and_fill_memory_buffers();
	set_kernel_args();
	start_kernel();
	read_output_image();
	save_image();
	free_resources();
	return EXIT_SUCCESS;
}


