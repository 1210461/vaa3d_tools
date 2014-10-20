/*
 * 2014.10.07 by: Hanbo Chen cojoc(at)hotmail.com
*/

#ifndef NEURON_STITCH_FUNC_H
#define NEURON_STITCH_FUNC_H

#include <basic_surf_objs.h>
#include <basic_landmark.h>

#define NTDIS(a,b) (((a).x-(b).x)*((a).x-(b).x)+((a).y-(b).y)*((a).y-(b).y)+((a).z-(b).z)*((a).z-(b).z))

double distance_XYZList(QList<XYZ> c0, QList<XYZ> c1);
void rotation_XYZList(QList<XYZ> in, QList<XYZ>& out, double angle, int axis); //angle is 0-360  based
bool compute_affine_4dof(QList<XYZ> reference, QList<XYZ> tomove, double& shift_x, double& shift_y, double & shift_z, double & angle_r, double & cent_x,double & cent_y,double & cent_z,int dir);

void update_marker_info(const LocationSimple& mk, int* info); //info[0]=neuron id, info[1]=point id, info[2]=matching marker
bool get_marker_info(const LocationSimple& mk, int* info);

void getNeuronTreeBound(const NeuronTree& nt, float * bound, int direction);


void getNeuronTreeBound(const NeuronTree& nt, double &minx, double &miny, double &minz,
                        double &maxx, double &maxy, double &maxz,
                        double &mmx, double &mmy, double &mmz);

int highlight_edgepoint(const QList<NeuronTree> *ntList, float dis, int direction);

int highlight_adjpoint(const NeuronTree& nt1, const NeuronTree& nt2, float dis);

void change_neuron_type(const NeuronTree& nt, int type);

void copyType(QList<int> source, const NeuronTree & target);

void copyType(const NeuronTree & source, QList<int> & target);

void backupNeuron(const NeuronTree & source, const NeuronTree & backup);

void copyProperty(const NeuronTree & source, const NeuronTree & target);

float quickMoveNeuron(QList<NeuronTree> * ntTreeList, int ant, int stackdir, int first_nt);

void multiplyAmat(double* front, double* back);
void multiplyAmat_centerRotate(double* rotate, double* tmat, double cx, double cy, double cz);

bool writeAmat(const char* fname, double* amat);
bool readAmat(const char* fname, double* amat);

bool export_list2file(QList<NeuronSWC> & lN, QString fileSaveName, QString fileOpenName);
#endif // NEURON_STITCH_FUNC_H