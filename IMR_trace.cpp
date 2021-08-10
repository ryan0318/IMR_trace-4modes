#include "IMR_trace.h"
using namespace std;
long long *map; //store pba
long long *track_write;
fstream outfile;
int mode;
long long write_pos=1; //write possition

int main(int argc, char *argv[]) {
	if (argc != 4) {
		cout << "?error: wrong argument number" << endl;
		exit(1);
	}//IMR_trace.exe test0713.trace 1 outfile.out
	
	create_map();

	runtrace(argv);
	cout << "run finish" << endl;
	double used = 0;
	for (int i = 1; i < LBATOTAL + 1; i++) {
		if (map[i] != -1) {
			used++;
		}
	}
	double tn = LBATOTAL;
	double ratio = used / tn;
	//cout << "total sector used: " << used << "/" << LBATOTAL << " " << ratio * 100 << "%" << endl;
	used = 0;
	for (int i = 0; i < TRACK_NUM; i++) {
		if (track_write[i] != 0) {
			used++;
		}
	}
	tn = TRACK_NUM;
	ratio = used / tn;
	cout << "total tracks used: " << used << "/" << TRACK_NUM << " " << ratio * 100 << "%" << endl;
	
	exit(0);
}


void create_map()
{
	map = new long long[LBATOTAL+1];	//establish map
	track_write = new long long[TRACK_NUM];	//establish track write record
	for (int i = 0; i < LBATOTAL + 1; i++) {
		map[i] = -1;
	}
	for (int i = 0; i < TRACK_NUM; i++) {
		track_write[i] = 0;
	}
}

void runtrace(char **argv){
	
	mode = stoi(argv[2]);		//mode
	
	fstream infile;
	
	infile.open(argv[1], ios::in);	//input file
	
	outfile.open(argv[3], ios::out);	//output file
	
	access temp;
	cout << "Start" << endl;
	while (infile>>temp.time>>temp.iotype>>temp.address>>temp.size) {
		
		temp.time *= 1000;
		
		if (temp.iotype=='R'|| temp.iotype == '1') {	//it's read request
			read(temp);
		}
		else {	//it's write request
			
/*			if (track_write[1] != 0) { cout << "Start writing top since: " << temp.time << endl;
			system("pause");
			}*/
			temp.iotype = '0';
			if (mode == SEQUENTIAL_IN_PLACE || mode == SEQUENTIAL_OUT_PLACE)
				seqtrack_write(temp, mode);
			else if (mode == CROSSTRACK_IN_PLACE || mode == CROSSTRACK_OUT_PLACE)
				crosstrack_write(temp, mode);
			
		}
		
	}
	cout << "OK" << endl;
	infile.close();
	outfile.close();
	
}

void read(access request){	//may need more error handling
	request.iotype = '1';
	vector <access>reading;
	reading.push_back(request);
	reading[0].size = 0;
	if (get_pba(request.address) != -1)
		reading[0].address = get_pba(request.address);
	else
		reading[0].address = request.address;
	long long prev_pba = -1;
	long long target;
	for (int i = 0; i < request.size; i++) {
		if (get_pba(request.address + i) != -1)
			target = get_pba(request.address + i);
		else
			target = request.address + i;

		if ((target == (prev_pba + 1)) || i == 0) { //if target pba is sequential
			reading[reading.size() - 1].size++;
		}
		else {		//if target pba is not sequential
			reading.push_back(request);
			reading[reading.size() - 1].size = 1;
			reading[reading.size() - 1].address = target;
		}
		prev_pba = target;

	}

	for (int i = 0; i < reading.size(); i++) {	//print read trace
		writefile(reading[i]);
	}

}

void seqtrack_write(access &request, int mode){	//still need to store lba info
	vector <access>result;

	access write_temp = request;
	access read_temp = request;
	read_temp.iotype = '1';
	long long pba;
	long long prev_pba = -1;
	long long prev_pba2 = -1;	//Second temp variable
	long long prev_pba3 = -1;	//third temp variable
	
	for (int i = 0; i < request.size; i++) {

		if (get_pba(request.address+i) == -1) {	//if pba didn't already exist (is not an update request)
			if (isTop(write_pos) || track(write_pos) == 0) {
				write_temp.address = write_pos;
				write_temp.size = 1;
				result.push_back(write_temp);
				track_write[track(write_pos)] = 1;
			}
			else {	//write bottom tracks
				if (track(write_pos) == track(prev_pba)) {
					write_temp.address = write_pos;
					write_temp.size = 1;
					result.push_back(write_temp);
					track_write[track(write_pos)] = 1;
				}
				else {	//need to read top track
					read_temp.address = track_head(track(write_pos)-1);
					read_temp.size = SECTORS_PER_TOPTRACK;  //(need to rewrite code if top track size isn't fixed)
					result.push_back(read_temp);

					write_temp.address = write_pos;
					write_temp.size = 1;
					result.push_back(write_temp);
					track_write[track(write_pos)] = 1;
				}

				if (i == (request.size - 1) || (track(write_pos + 1)) != track(write_pos)	//write back top track 
					|| get_pba(request.address + i + 1) != -1) { //if next access sector is update
					write_temp.address = track_head(track(write_pos) - 1);
					write_temp.size = SECTORS_PER_TOPTRACK;  //(need to rewrite code if top track size isn't fixed)
					result.push_back(write_temp);
				}
				prev_pba = write_pos;

			}
			map[request.address + i] = write_pos; //save lba
			write_pos++;
		}
		else {			//if it's update
			pba = get_pba(request.address + i);

			if (mode == SEQUENTIAL_IN_PLACE) {

				if (isTop(pba)) {
					write_temp.address = pba;
					write_temp.size = 1;
					result.push_back(write_temp);
				}
				else {	//Bottom update, need RMW

					if (track(pba) == track(prev_pba2)) {	//neighbor top tracks already read
						write_temp.address = pba;
						write_temp.size = 1;
						result.push_back(write_temp);
					}
					else {
						//need to read top track
						if (track(pba) != 0) {
							if (track_write[track(pba) - 1] != 0) {
								read_temp.address = track_head(track(pba) - 1);
								read_temp.size = SECTORS_PER_TOPTRACK;  //(need to rewrite code if track size isn't fixed)
								result.push_back(read_temp);
							}
						}
						if (track_write[track(pba) + 1] != 0) {
							read_temp.address = track_head(track(pba) + 1);
							read_temp.size = SECTORS_PER_TOPTRACK;  //(need to rewrite code if track size isn't fixed)
							result.push_back(read_temp);
						}
						write_temp.address = pba;
						write_temp.size = 1;
						result.push_back(write_temp);
					}

					if (i == (request.size - 1) || track(get_pba(request.address + i + 1)) != track(pba)) { //write back top track 
						if (track(pba) != 0) {
							if (track_write[track(pba) - 1] != 0) {
								write_temp.address = track_head(track(pba) - 1);
								write_temp.size = SECTORS_PER_TOPTRACK;  //(need to rewrite code if track size isn't fixed)
								result.push_back(write_temp);
							}
						}
						if (track_write[track(pba) + 1] != 0) {
							write_temp.address = track_head(track(pba) + 1);
							write_temp.size = SECTORS_PER_TOPTRACK;  //(need to rewrite code if track size isn't fixed)
							result.push_back(write_temp);
						}
					}
					

				}
				prev_pba2 = pba;
			}
			else {		//out place update
				if (isTop(write_pos) || track(write_pos) == 0) {	 //write top track
					write_temp.address = write_pos;
					write_temp.size = 1;
					result.push_back(write_temp);
					track_write[track(write_pos)] = 1; //save track write record
				}
				else {  //write bottom track
					if (track(write_pos) == track(prev_pba3)) {
						write_temp.address = write_pos;
						write_temp.size = 1;
						result.push_back(write_temp);
					}
					else {	//need to read top track
						read_temp.address = track_head(track(write_pos) - 1);
						read_temp.size = SECTORS_PER_TOPTRACK;  //(need to rewrite code if track size isn't fixed)
						result.push_back(read_temp);

						write_temp.address = write_pos;
						write_temp.size = 1;
						result.push_back(write_temp);

					}

					if (i == (request.size - 1) || track(get_pba(request.address + i + 1)) != track(pba)) { //write back top track 
						write_temp.address = track_head(track(write_pos) - 1);
						write_temp.size = SECTORS_PER_TOPTRACK;  //(need to rewrite code if track size isn't fixed)
						result.push_back(write_temp);
					}
					prev_pba3 = write_pos;


				}
				map[request.address + i] = write_pos; //save new lba
				write_pos++;
			}
		}
	}
	
	//output the trace
	vector <access>output;
	access out_temp = request;
	out_temp.size = result[0].size;
	out_temp.address = result[0].address;
	out_temp.iotype = result[0].iotype;
	output.push_back(out_temp);
	prev_pba = result[0].address;
	/*Packing trace*/
	for (int i = 1; i < result.size(); i++) {	
		if (result[i].iotype == '1') {
			output.push_back(result[i]);
		}
		else {
			if (result[i].address == (prev_pba + 1) && track(result[i].address) == track(prev_pba) && result[i - 1].iotype == '0') {
				output[output.size() - 1].size++;		//if pba is sequential to its previous write pba
			}
			else {	//if pba is not sequential
				out_temp.address = result[i].address;
				out_temp.size = result[i].size;
				out_temp.iotype = '0';
				output.push_back(out_temp);
			}
			prev_pba = result[i].address;
		}
			
	}

	//print output
	for (int i = 0; i < output.size(); i++) {	
		writefile(output[i]);
	}
	
}

void crosstrack_write(access &request, int mode){
	vector <access>result;
	access write_temp = request;
	access read_temp = request;
	read_temp.iotype = '1';
	long long pba;
	long long prev_pba = -1;
	for (int i = 0; i < request.size; i++) {
		if (get_pba(request.address + i) == -1) {//if pba didn't already exist (is not an update request)
			write_temp.address = write_pos;
			write_temp.size = 1;
			result.push_back(write_temp);
			track_write[track(write_pos)] = 1; //save track write record
			map[request.address + i] = write_pos; //save lba

			if (track(write_pos + 1) != track(write_pos)) {
				write_pos = track_head(track(write_pos) + 2);
				if (!isTop(write_pos) && track(write_pos) > TRACK_NUM - 1) {
					write_pos= track_head(1);
				}
			}
			else
				write_pos++;
		} 
		else {		//if it's update
			pba = get_pba(request.address + i);

			if (mode == CROSSTRACK_IN_PLACE) {
				if (isTop(pba)) {
					write_temp.address = pba;
					write_temp.size = 1;
					result.push_back(write_temp);
				}
				else {	//Bottom update, need RMW

					if (track(pba) == track(prev_pba)) {	//neighbor top tracks already read
						write_temp.address = pba;
						write_temp.size = 1;
						result.push_back(write_temp);
					}
					else {
						//need to read top track
						if (track(pba) != 0) {
							if (track_write[track(pba) - 1] != 0) {
								read_temp.address = track_head(track(pba) - 1);
								read_temp.size = SECTORS_PER_TOPTRACK;  //(need to rewrite code if track size isn't fixed)
								result.push_back(read_temp);
							}
						}
						if (track_write[track(pba) + 1] != 0) {
							read_temp.address = track_head(track(pba) + 1);
							read_temp.size = SECTORS_PER_TOPTRACK;  //(need to rewrite code if track size isn't fixed)
							result.push_back(read_temp);
						}
						write_temp.address = pba;
						write_temp.size = 1;
						result.push_back(write_temp);
					}

					if (i == (request.size - 1) || track(get_pba(request.address + i + 1)) != track(pba)) { //write back top track 
						if (track(pba) != 0) {
							if (track_write[track(pba) - 1] != 0) {
								write_temp.address = track_head(track(pba) - 1);
								write_temp.size = SECTORS_PER_TOPTRACK;  //(need to rewrite code if track size isn't fixed)
								result.push_back(write_temp);
							}
						}
						if (track_write[track(pba) + 1] != 0) {
							write_temp.address = track_head(track(pba) + 1);
							write_temp.size = SECTORS_PER_TOPTRACK;  //(need to rewrite code if track size isn't fixed)
							result.push_back(write_temp);
						}
					}


				}
				prev_pba = pba;
			}
			else {		//outplace update if bottom
				if (isTop(pba) || (track(pba) == 0 && track_write[track(pba) + 1] == 0)) {
					write_temp.address = pba;
					write_temp.size = 1;
					result.push_back(write_temp);
				}
				else if (track_write[track(pba) - 1] == 0 && track_write[track(pba) + 1] == 0) {
					write_temp.address = pba;
					write_temp.size = 1;
					result.push_back(write_temp);
				}
				else {
					write_temp.address = write_pos;
					write_temp.size = 1;
					result.push_back(write_temp);
					track_write[track(write_pos)] = 1; //save track write record
					map[request.address + i] = write_pos; //save lba
					/*moving write position*/
					if (track(write_pos + 1) != track(write_pos)) {
						write_pos = track_head(track(write_pos) + 2);
						if (!isTop(write_pos) && track(write_pos) > TRACK_NUM - 1) {
							write_pos = track_head(1);
						}
					}
					else
						write_pos++;
				}
			}
		}
	}

	vector <access>output;
	access out_temp = request;
	out_temp.size = result[0].size;
	out_temp.address = result[0].address;
	out_temp.iotype = result[0].iotype;
	output.push_back(out_temp);
	prev_pba = result[0].address;

	for (int i = 1; i < result.size(); i++) {
		if (result[i].iotype == '1') {
			output.push_back(result[i]);
		}
		else {
			if (result[i].address == (prev_pba + 1) && track(result[i].address) == track(prev_pba) && result[i - 1].iotype == '0') {
				output[output.size() - 1].size++;		//if pba is sequential to its previous write pba
			}
			else {	//if pba is not sequential
				out_temp.address = result[i].address;
				out_temp.size = result[i].size;
				out_temp.iotype = '0';
				output.push_back(out_temp);
			}
			prev_pba = result[i].address;
		}
	}

	for (int i = 0; i < output.size(); i++) {
		writefile(output[i]);
	}

}

void writefile(access Access){
	outfile << Access.time << "\t" << Access.device << "\t"
		<< Access.address << "\t" << Access.size << "\t" << Access.iotype << endl;
}

long long get_pba(long long lba)
{
	return map[lba];
}

long long track(long long pba)
{		//Track start from 0, LBA start from 1
	if (pba == -1)
		return -1;

	long long n = pba / (SECTORS_PER_BOTTRACK + SECTORS_PER_TOPTRACK);
	if (pba - n * (SECTORS_PER_BOTTRACK + SECTORS_PER_TOPTRACK) > SECTORS_PER_BOTTRACK)
		return (2 * n + 1);
	else if (pba - n * (SECTORS_PER_BOTTRACK + SECTORS_PER_TOPTRACK) > 0)
		return 2 * n;
	else
		return (2 * n - 1);
}

long long track_head(long long t)
{						// if LBA start from 1

	if (t % 2 != 0) {	//if isTop
		return (t / 2 * (SECTORS_PER_BOTTRACK + SECTORS_PER_TOPTRACK)) + SECTORS_PER_BOTTRACK + 1;
	}
	else {
		return (t / 2 * (SECTORS_PER_BOTTRACK + SECTORS_PER_TOPTRACK)) + 1;
	}
}

bool isTop(long long pba)
{

	if (pba == -1)
		return false;
	if (track(pba) % 2 != 0)
		return true;
	return false;


}

