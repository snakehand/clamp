/*****************************************************************
    This file is part of Clamp.

    Clamp is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License.

    Clamp is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Clamp.  If not, see <http://www.gnu.org/licenses/>.
********************************************************************/

#include <utility>
#define __NO_STD_VECTOR // Use cl::vector instead of STL version
#include <CL/cl.hpp>

#include "Clamp.h"
#include <iostream>
#include <stdlib.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <iterator>
#include <stdio.h>

/********************************************
 *
 *  Clamp - host class
 *
 *******************************************/
Clamp::Clamp() :
	mNumDevices(0),
	mCreatedDevices(NULL)
{
    cl_int err;

    cl::Platform::get(&mPlatformList);
    checkErr(mPlatformList.size()!=0 ? CL_SUCCESS : -1, "cl::Platform::get");
    std::cerr << "Platform number is: " << mPlatformList.size() << std::endl;
    
    std::string platformVendor;
    mPlatformList[0].getInfo((cl_platform_info)CL_PLATFORM_VENDOR,
                            &platformVendor);
    std::cerr << "Platform is by: " << platformVendor << "\n";

	cl_context_properties cprops[3] = 
        {CL_CONTEXT_PLATFORM, (cl_context_properties)(mPlatformList[0])(), 0};

    mCtx = new cl::Context(
        CL_DEVICE_TYPE_GPU, 
        cprops,
        NULL,
        NULL,
        &err);

    /* Obtain list of devices */
    mDeviceList = mCtx->getInfo<CL_CONTEXT_DEVICES>();
    checkErr(mDeviceList.size() > 0 ? CL_SUCCESS : -1, "devices.size() > 0");

	mNumDevices = mDeviceList.size();
	mCreatedDevices = new ClDev* [mNumDevices];
	memset(mCreatedDevices,0,mNumDevices*sizeof(ClDev*));
}

Clamp::~Clamp()
{
	if (mCreatedDevices) {
		for (int i=0; i<mNumDevices; i++) {
			delete mCreatedDevices[i];
			mCreatedDevices[i] = NULL;
		}
		delete mCreatedDevices;
		mCreatedDevices = NULL;
	}
	delete mCtx;
}

void Clamp::checkErr(cl_int err, const char * name)
{
    if (err != CL_SUCCESS) {
        std::cerr << "ERROR: " << name
                  << " (" << err << ")" << std::endl;
        exit(EXIT_FAILURE);
    }
}

ClDev* Clamp::createDev(int num)
{
	if (mCreatedDevices==NULL) return NULL;
	if (num<0) return NULL;
	if (num>=mNumDevices) return NULL;

	if (mCreatedDevices[num]==NULL) {	
		ClDev* dev = new ClDev(this,&mDeviceList[num],mCtx);
		mCreatedDevices[num] = dev;
        // cl_int cSize =  dev->getInfo(
	}

	return mCreatedDevices[num];
}

ClProgram* Clamp::compileProgram(const char* path)
{
	std::ifstream file(path);

	if (!file.is_open()) {
		fprintf(stdout, "Could not read CL program %s\n", path);
		return NULL;
	}

	std::string prog(
		std::istreambuf_iterator<char>(file),
		(std::istreambuf_iterator<char>()));

	cl::Program::Sources source(
		1,
		std::make_pair(prog.c_str(), prog.length()+1));

	cl::Program* program = new cl::Program(*mCtx, source);
	cl_int err = program->build(mDeviceList,"-cl-fast-relaxed-math"); 
	file.close();
    if (err != CL_SUCCESS)	{
		std::string bi;
		program->getInfo(CL_PROGRAM_BUILD_LOG, &bi);
		std::cout << "Errror:\n" << bi;

        checkErr(err, "Program::build()"); 
    } else {
		ClProgram* prg = new ClProgram(program);
		return prg;
	}

	return NULL;
}

ClProgram* Clamp::compileProgramString(std::string prog)
{
	cl::Program::Sources source(
		1,
		std::make_pair(prog.c_str(), prog.length()+1));

	cl::Program* program = new cl::Program(*mCtx, source);
	cl_int err = program->build(mDeviceList,"-cl-fast-relaxed-math"); 

    if (err != CL_SUCCESS)	{
		std::string bi;
		program->getInfo(CL_PROGRAM_BUILD_LOG, &bi);
		std::cout << "Errror:\n" << bi;

        checkErr(err, "Program::build()"); 
    } else {
		ClProgram* prg = new ClProgram(program);
		return prg;
	}

	return NULL;
}


/*************************************************
 *
 *  ClDev device class
 *
 ************************************************/
ClDev::ClDev(Clamp* clamp, cl::Device* dev, cl::Context* ctx) :
	mClamp(clamp),
	mDev(dev),
	mCtx(ctx),
	mQueue(NULL)
{
	cl_int err;
	mQueue = new cl::CommandQueue(*mCtx, *mDev, 0, &err);
    Clamp::checkErr(err, "CommandQueue::CommandQueue()");
	mEvent = new cl::Event(); 
}

ClDev::~ClDev()
{
	delete mQueue;
	delete mEvent;
}

ClMem* ClDev::allocMem(size_t size,int dimx, int dimy)
{
	cl::Buffer* buf = NULL;
	cl_int err;
	char* bogus = new char[size*dimx*dimy];
	buf = new cl::Buffer(*mCtx,CL_MEM_READ_WRITE,size*dimx*dimy,NULL,&err);
	Clamp::checkErr(err,"ClDev::allocMem");

	if (buf) {
		ClMem* mem = new ClMem();
		mem->mBuf = buf;
		mem->mSize = size*dimx*dimy;
		mem->mBogus = bogus;
		mMemory.push_back(mem);
		return mem;
	}
	return NULL;
}

void ClDev::queue(ClKernel* kernel)
{
	cl_int err;
	if (kernel->mY) {
		/* 2D run */
		err = mQueue->enqueueNDRangeKernel(
			*(kernel->mKernel), 
			cl::NullRange,
			cl::NDRange(kernel->mX,kernel->mY),
			cl::NDRange(1, 1), 
			NULL, 
			mEvent);
	} else {
		/* 1D run */
		err = mQueue->enqueueNDRangeKernel(
			*(kernel->mKernel), 
			cl::NullRange,
			cl::NDRange(kernel->mX),
			cl::NDRange(1, 1), 
			NULL, 
			mEvent);
	}
    Clamp::checkErr(err, "ComamndQueue::enqueueNDRangeKernel()");

}

void ClDev::wait()
{
	mEvent->wait();
}

void ClDev::read(ClMem* m, void*out)
{
	cl_int err = mQueue->enqueueReadBuffer(
        *(m->mBuf),
        CL_TRUE,
        0,
        m->mSize,
        out);
    Clamp::checkErr(err, "ComamndQueue::enqueueReadBuffer()");
}

void ClDev::read(ClMem* m)
{
	read(m,m->mBogus);
}


void ClDev::write(ClMem* m, void* in)
{
	cl_int err = mQueue->enqueueWriteBuffer(
        *(m->mBuf),
        CL_TRUE,
        0,
        m->mSize,
        in);
    Clamp::checkErr(err, "ComamndQueue::enqueueWriteBuffer()");
}

void ClDev::write(ClMem* m)
{
	write(m,m->mBogus);
}



void ClDev::freeMem(ClMem* m)
{
	std::list<ClMem*>::iterator it = mMemory.begin();
	while (it!=mMemory.end()) {
		if ((*it)==m) {
			delete m;
			mMemory.erase(it);
			return;
		}
	}

	std::cerr << "ClDev::freeMem() " << (void*)m << " not found." << std::endl;
}

/*******************************************
 *
 * ClMem memory buffer helper class
 *
 ******************************************/
ClMem::ClMem() :
	mBuf(0),
	mSize(0),
	mBogus(0)
{
}

ClMem::~ClMem()
{
	delete mBuf;
	delete mBogus;
}

void ClMem::Clear()
{
    if (mBogus&&mSize) {
        memset(mBogus,0,mSize);
    }
}

/*******************************************
 *
 * ClProgram buffer helper class
 *
 ******************************************/
ClProgram::ClProgram(cl::Program* prog) :
	mProg(prog)
{
}

ClProgram::~ClProgram()
{
	std::list<ClKernel*>::iterator it = mKernels.begin();
	while (it!=mKernels.end()) {
		delete (*it);
		it++;
	}
	delete mProg;
}

ClKernel* ClProgram::getKernel(const char* name)
{
	cl_int err;
	cl::Kernel* kernel = new cl::Kernel(*mProg, name, &err);
 	if (err==CL_SUCCESS) {
		ClKernel* krn = new ClKernel(kernel);
		mKernels.push_back(krn);
		return krn;
	}
	delete kernel;
	return NULL;
}


/*******************************************
 *
 * ClKernel helper class
 *
 ******************************************/
void ClKernel::setArg(int num, ClMem* mem)
{
	cl_int err;
	err = mKernel->setArg(num, *mem->mBuf);
	Clamp::checkErr(err, "Kernel::setArg()");
}

void ClKernel::setArg(int num, int arg)
{
	cl_int err;
	err = mKernel->setArg(num, arg);
	Clamp::checkErr(err, "Kernel::setArg()");
}

void ClKernel::setArg(int num, float arg)
{
	cl_int err;
	err = mKernel->setArg(num, arg);
	Clamp::checkErr(err, "Kernel::setArg()");
}

void ClKernel::setDomain(int sx)
{
	mX = sx;
	mY = 0;
	mZ = 0;
}

void ClKernel::setDomain(int sx, int sy)
{
	mX = sx;
	mY = sy;
	mZ = 0;
}


ClKernel::ClKernel(cl::Kernel* k) :
	mKernel(k),
	mX(0),
	mY(0),
	mZ(0)
{
}

ClKernel::~ClKernel()
{
	delete mKernel;
}
