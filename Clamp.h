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

#ifndef _CLAMP_H_
#define _CLAMP_H_

#include <utility>
#define __NO_STD_VECTOR // Use cl::vector instead of STL version
#include <CL/cl.hpp>

#include<list>

class Clamp;
class ClDev;
class ClProgram;
class ClKernel;

/**
 * Wrap a CL memory object
 */
class ClMem {
public:
	cl::Buffer* getBuffer() {return mBuf;}
	void* getHostBuffer() {return (void*)mBogus;}
    void Clear();

private:
	friend class ClDev;
	friend class ClKernel;
	ClMem();
	~ClMem();

	cl::Buffer* mBuf;
	size_t mSize;
	char* mBogus;
};

class ClKernel {
public:
	void setArg(int num, ClMem* mem);
	void setArg(int num, int arg);
	void setArg(int num, float arg);
	void setDomain(int);
	void setDomain(int,int);
	// void setDomain(int,int,int);
	
	cl::Kernel* getKernel() { return mKernel; }

private:
	friend class ClProgram;
	friend class ClDev;
	ClKernel(cl::Kernel* k);
	~ClKernel();

	cl::Kernel* mKernel;
	int mX;
	int mY;
	int mZ;
};

class ClProgram {
public:
	ClKernel* getKernel(const char* name);

	cl::Program* getProgram() {return mProg;}

private:
	friend class Clamp;
	ClProgram(cl::Program*);
	~ClProgram();

	cl::Program* mProg;
	std::list<ClKernel*> mKernels;
};



/**
 * Wrap an OpenCl device & context
 */
class ClDev {
public:
	ClMem* allocMem(size_t size,int dimx, int dimy=1);
	void freeMem(ClMem* m);
	void queue(ClKernel* k);
	void wait();
	void read(ClMem* m, void*out);
	void read(ClMem* m);
	void write(ClMem* m, void* in);
	void write(ClMem* m);

	cl::Device* getDevice() {return mDev;}
	cl::CommandQueue* getQueue() {return mQueue;}

private:
    friend class Clamp;
    ClDev(Clamp*, cl::Device*, cl::Context*);
    ~ClDev();

	Clamp* mClamp;
	cl::Device  *mDev;
    cl::Context *mCtx;
    std::list<ClMem*> mMemory;

	cl::CommandQueue* mQueue;
    cl::Event* mEvent;

};


/**
 * Wrap an OpenCl environment
 */
class Clamp {
public:
    Clamp();
    ~Clamp();

	int getNumDevices() {return mNumDevices;}
    ClDev* createDev(int num);
    static void checkErr(cl_int err, const char * name);

	ClProgram* compileProgram(const char* path);
	ClProgram* compileProgramString(std::string prog);

	// Debug
	cl::Context* getContext() {return mCtx;}

private:
	cl::Context *mCtx;

	cl::vector<cl::Platform> mPlatformList;
    cl::vector<cl::Device> mDeviceList;

	int mNumDevices;
	ClDev** mCreatedDevices;
};


#endif
