#ifndef IMR_TRACE_H
#define IMR_TRACE_H
//IMR_trace_sys17_2_new 72602933/90560800 used sectors 46501 tracks
//IMR_trace_sys17_4_new: 82599504 used sectors
#define LBATOTAL 103219550
#define TRACK_NUM 53001
#define SECTORS_PER_BOTTRACK 2050
#define SECTORS_PER_TOPTRACK 1845
#define TOTAL_BOTTRACK	26501
#define TOTAL_TOPTRACK	26500

#define SEQUENTIAL_IN_PLACE 1
#define SEQUENTIAL_OUT_PLACE 2
#define CROSSTRACK_IN_PLACE 3
#define CROSSTRACK_OUT_PLACE 4
#include<iostream>
#include<fstream>
#include<stdlib.h>
#include<vector>
#include<string>

struct access {
	long long time;
	char iotype;
	long long address;  //lba, may be pba when write file
//	long long pba = 0;
	int size;
	int device = 0;
};

void create_map();
void runtrace(char **argv);
void read(access request);
void seqtrack_write(access &request, int mode);
void crosstrack_write(access &request, int mode);
void writefile(access Access);
long long get_pba(long long lba);
long long track(long long pba); //return which track is the pba on
long long track_head(long long track);
//bool isUpdate(access request);
bool isTop(long long pba);

#endif