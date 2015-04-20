#include <string>
#include "nt_selfcorrect_func.h"
#include <stdio.h>
#include "hang/topology_analysis.h"
#include "mrmr/mrmr.h"

#define Malloc(type,n) (type *)malloc((n)*sizeof(type))

nt_selfcorrect_func::nt_selfcorrect_func()
{
    p_img1D = 0;
    ppp_img3D = 0;
    tmp_ppp_window = 0;
    sz_img = 0;
    type_img = 1;
    featureNum = 0;
    svmModel = 0;
    svmNode = 0;
    initParameter();
}

bool nt_selfcorrect_func::loadData(QString fname_img, QString fname_swc)
{
    char* cstr;
    string fname = fname_img.toStdString();
    cstr = new char [fname.size()+1];
    strcpy( cstr, fname.c_str() );
    //load image
    if(!loadImage(cstr, p_img1D, sz_img, type_img)){
        qDebug()<<"error: cannot read image "<<fname_img;
        return false;
    }
    delete [] cstr;
    if(sz_img[3]>1){
        qDebug()<<"warning: image has more than 1 color channel. Only the first channel will be used.";
        sz_img[3]=1;
    }
    //arrange the image into 3D
    ppp_img3D=new unsigned char ** [sz_img[2]];
    for(V3DLONG z=0; z<sz_img[2]; z++){
        ppp_img3D[z]=new unsigned char * [sz_img[1]];
        for(V3DLONG y=0; y<sz_img[1]; y++){
            ppp_img3D[z][y]=p_img1D+y*sz_img[0]+z*sz_img[0]*sz_img[1];
        }
    }

    //load neuron tree
    ntmarkers = readSWC_file(fname_swc.toStdString());
}

bool nt_selfcorrect_func::saveData(QString fname_output){
    QString fname_out;
    fname_out=fname_output+"_predict.swc";
    saveSWC_file(fname_out.toStdString(), ntmarkers);

    return true;
}

bool nt_selfcorrect_func::calculateScore()
{
    if(type_img==1)
        topology_analysis3(p_img1D, ntmarkers, score_map, sz_img[0], sz_img[1], sz_img[2]);
    else if(type_img==2)
        topology_analysis3((unsigned short *)p_img1D, ntmarkers, score_map, sz_img[0], sz_img[1], sz_img[2]);
    else if(type_img==4)
        topology_analysis3((float *)p_img1D, ntmarkers, score_map, sz_img[0], sz_img[1], sz_img[2]);

    //for test
//    double max_score=0, min_score=MAX_DOUBLE;
//    for(V3DLONG i = 0; i<ntmarkers.size(); i++){
//        MyMarker * marker = ntmarkers[i];
//        max_score = MAX(max_score, score_map[marker]);
//        min_score = MIN(min_score, score_map[marker]);
//    }
    for(V3DLONG i = 0; i<ntmarkers.size(); i++){
        MyMarker * marker = ntmarkers[i];
        marker->type = score_map[marker] * 10 +19;
    }
    string fname_tmp = "neuronTree_score.swc";
    saveSWC_file(fname_tmp, ntmarkers);
}

bool nt_selfcorrect_func::getTrainingSample()
{
    double STEP=0.4;
    train_positive_idx.clear();
    train_negative_idx.clear();

    unsigned char * p_mask1D=new unsigned char[sz_img[0]*sz_img[1]*sz_img[2]];
    memset(p_mask1D,0,sz_img[0]*sz_img[1]*sz_img[2]);
    //mask the neighbor defined by radius and radiusFactor
    //p_mask1D[*]=2: inter area
    for(V3DLONG nid=0; nid<ntmarkers.size(); nid++){
        MyMarker* current = ntmarkers.at(nid);
        MyMarker* parent = current->parent;
        if(parent<=0){
            continue; //continue if it has not parent
        }
        double xf,yf,zf,rf;
        int xi,yi,zi,ri;
        double xv=parent->x-current->x;
        double yv=parent->y-current->y;
        double zv=parent->z-current->z;
        double rv=parent->radius-current->radius;
        double len=sqrt(xv*xv+yv*yv+zv*zv);

        for(double lenAccu=0; lenAccu<len; lenAccu+=STEP){
            xf=current->x+xv*lenAccu/len;
            yf=current->y+yv*lenAccu/len;
            zf=current->z+zv*lenAccu/len;
            rf=(current->radius+rv*lenAccu/len)*param.sample_radiusFactor_inter;
            ri=rf+0.5;
            for(int dx=-ri; dx<=ri; dx++){
                for(int dy=-ri; dy<=ri; dy++){
                    for(int dz=-ri; dz<=ri; dz++){
                        if(dx*dx+dy*dy+dz*dz>rf*rf){
                            continue;
                        }
                        xi=xf+dx;
                        yi=yf+dy;
                        zi=zf+dz;
                        if(xi<0 || xi>=sz_img[0])
                            continue;
                        if(yi<0 || zi>=sz_img[1])
                            continue;
                        if(zi<0 || zi>=sz_img[2])
                            continue;
                        p_mask1D[xi+yi*sz_img[0]+zi*sz_img[0]*sz_img[1]]=2;
                    }
                }
            }
        }
    }

    //mask the training area defined by radius and radiusFactor
    //p_mask1D[*]=3: negative sample
    //p_mask1D[*]=1: positive sample
    V3DLONG negativeCount=0;
    V3DLONG positiveCount=0;
    for(V3DLONG nid=0; nid<ntmarkers.size(); nid++){
        MyMarker* current = ntmarkers.at(nid);
        MyMarker* parent = current->parent;
        if(parent<=0){
            continue; //continue if it has not parent
        }
        if(score_map[current]>param.sample_scoreThr || score_map[parent]>param.sample_scoreThr){
            continue; //continue if the tracing is not trustworthy
        }
        double xf,yf,zf,rf;
        int xi,yi,zi,ri;
        double xv=parent->x-current->x;
        double yv=parent->y-current->y;
        double zv=parent->z-current->z;
        double rv=parent->radius-current->radius;
        double len=sqrt(xv*xv+yv*yv+zv*zv);

        for(double lenAccu=0; lenAccu<len; lenAccu+=STEP){
            xf=current->x+xv*lenAccu/len;
            yf=current->y+yv*lenAccu/len;
            zf=current->z+zv*lenAccu/len;

            //positive sample
            rf=(current->radius+rv*lenAccu/len)*param.sample_radiusFactor_positive;
            ri=rf+0.5;
            for(int dx=-ri; dx<=ri; dx++){
                for(int dy=-ri; dy<=ri; dy++){
                    for(int dz=-ri; dz<=ri; dz++){
                        if(dx*dx+dy*dy+dz*dz>rf*rf){
                            continue;
                        }
                        xi=xf+dx;
                        yi=yf+dy;
                        zi=zf+dz;
                        if(xi<0 || xi>=sz_img[0])
                            continue;
                        if(yi<0 || zi>=sz_img[1])
                            continue;
                        if(zi<0 || zi>=sz_img[2])
                            continue;
                        if(p_mask1D[xi+yi*sz_img[0]+zi*sz_img[0]*sz_img[1]]!=1)
                            positiveCount++;
                        p_mask1D[xi+yi*sz_img[0]+zi*sz_img[0]*sz_img[1]]=1;
                    }
                }
            }

            //negative sample
            rf=(current->radius+rv*lenAccu/len)*param.sample_radiusFactor_negative;
            ri=rf+0.5;
            for(int dx=-ri; dx<=ri; dx++){
                for(int dy=-ri; dy<=ri; dy++){
                    for(int dz=-ri; dz<=ri; dz++){
                        if(dx*dx+dy*dy+dz*dz>rf*rf){
                            continue;
                        }
                        xi=xf+dx;
                        yi=yf+dy;
                        zi=zf+dz;
                        if(xi<0 || xi>=sz_img[0])
                            continue;
                        if(yi<0 || zi>=sz_img[1])
                            continue;
                        if(zi<0 || zi>=sz_img[2])
                            continue;
                        if(p_mask1D[xi+yi*sz_img[0]+zi*sz_img[0]*sz_img[1]]!=0){
                            continue;
                        }
                        negativeCount++;
                        p_mask1D[xi+yi*sz_img[0]+zi*sz_img[0]*sz_img[1]]=3;
                    }
                }
            }
        }
    }

    //get the trainging samples based on mask
    V3DLONG sampleNum=MIN(positiveCount,param.sample_maxNumber);
    sampleNum=MIN(negativeCount,sampleNum);
    double positiveSampleRate=(double)positiveCount/(double)sampleNum;
    double negativeSampleRate=(double)negativeCount/(double)sampleNum;
    V3DLONG positiveRec=0, negativeRec=0;
    double positiveLine=0, negativeLine=0;
    for(V3DLONG idx=0; idx<sz_img[0]*sz_img[1]*sz_img[2]; idx++){
        if(p_mask1D[idx]==1){//positive
            positiveRec++;
            if(positiveRec>=positiveLine){
                positiveLine+=positiveSampleRate;
                train_positive_idx.push_back(idx);
                p_mask1D[idx]=4;
            }
        }
        if(p_mask1D[idx]==3){//negative
            negativeRec++;
            if(negativeRec>=negativeLine){
                negativeLine+=negativeSampleRate;
                train_negative_idx.push_back(idx);
                p_mask1D[idx]=4;
            }
        }
    }

    //for test
    char fname_tmp[1000];
    sprintf(fname_tmp,"test.raw");
    saveImage(fname_tmp,p_mask1D, sz_img, 1);

    qDebug()<<"positive: "<<positiveCount<<"; negative:"<<negativeCount;
}

bool nt_selfcorrect_func::performTraining()
{
    qDebug()<<"performing training based on mrmr and svm, sample number: p:"
           <<train_positive_idx.size()<<", n:"<<train_negative_idx.size();
    //get wavelet features for mRMR
    qDebug()<<"calculate wavelet feature, level: "<<param.wavelet_level;
    V3DLONG z, x, y, page;
    vector<float> tmp_wave;
    float * train_data;
    int sample_idx=0;
    for(int i=0; i<train_positive_idx.size(); i++){
        ind2sub(x,y,z,train_positive_idx[i]);
//        qDebug()<<"positive: "<<i<<":"<<x<<":"<<y<<":"<<z;
        if(getWindowWavelet(x,y,z,tmp_wave)){
            if(sample_idx==0){ //init the memory on the first round
                sampleNum=train_positive_idx.size()+train_negative_idx.size();
                featureNum=tmp_wave.size();
                train_data = new float [sampleNum*(featureNum+1)]; //the first veriable is the label
            }
            if(featureNum!=tmp_wave.size()){
                qDebug()<<"error: number of wavelet features is not consistent!";
                return 0;
            }

            page=sample_idx*(featureNum+1);
            train_data[page]=VAL_FG; //label
            for(int j=0; j<tmp_wave.size(); j++){ //copy the features
                train_data[page+j+1]=tmp_wave[j];
            }
            sample_idx++;
        }else{
            qDebug()<<"error when training: x:"<<x<<"; y:"<<y<<"; z:"<<z;
            return 0;
        }
    }
    for(int i=0; i<train_negative_idx.size(); i++){
        ind2sub(x,y,z,train_negative_idx[i]);
//        qDebug()<<"negative: "<<i<<":"<<x<<":"<<y<<":"<<z;
        if(getWindowWavelet(x,y,z,tmp_wave)){
            if(sample_idx==0){ //init the memory on the first round
                sampleNum=train_positive_idx.size()+train_negative_idx.size();
                featureNum=tmp_wave.size();
                train_data = new float [sampleNum*(featureNum+1)]; //the first veriable is the label
            }
            if(featureNum!=tmp_wave.size()){
                qDebug()<<"error: number of wavelet features is not consistent!";
                return 0;
            }

            page=sample_idx*(featureNum+1);
            train_data[page]=VAL_BG; //label
            for(int j=0; j<tmp_wave.size(); j++){ //copy the features
                train_data[page+j+1]=tmp_wave[j];
            }
            sample_idx++;
        }else{
            qDebug()<<"error when training: x:"<<x<<"; y:"<<y<<"; z:"<<z;
            return 0;
        }
    }

    //for test
    ofstream fp("test_trainingFeatures.txt");
    for(V3DLONG i=0; i<sampleNum*(featureNum+1); i++){
        fp<<train_data[i];
        if((i+1)%(featureNum+1)==0){
            fp<<endl;
        }else{
            fp<<"\t";
        }
    }
    fp.close();

    //perform mRMR
    qDebug()<<"mRMR feature selection: "<<param.mrmr_feaNum;
    float * mrmr_data = new float [sampleNum*(featureNum+1)]; //the first veriable is the label
    memcpy(mrmr_data, train_data, sampleNum*(featureNum+1)*sizeof(float));
    long * p_sel_feature;
    p_sel_feature=runmrmr(mrmr_data, sampleNum, featureNum+1, param.mrmr_feaNum, param.mrmr_method, param.mrmr_discretizeBinNumber);
    if(p_sel_feature){
        selFeature.clear();
        qDebug()<<"mrmr: "<<param.mrmr_feaNum<<" features selected:";
        qDebug()<<"index\tlevel\tL/H\tx\ty\tz";
        for(int i=0; i<param.mrmr_feaNum; i++){
            int feaIdx=p_sel_feature[i]-1;
            selFeature.push_back(feaIdx);
            cout<<feaIdx;
            for(int j=0; j<5; j++){
                cout<<"\t"<<featureInfo[feaIdx][j];
            }
            cout<<endl;
        }
    }else{
        qDebug()<<"error: failed in performing mrmr feature selection";
        return 0;
    }

    //perform SVM training
    qDebug()<<"svm: performing model training";
    //init problem
    if(svmModel!=0){
        svm_free_and_destroy_model(&svmModel);
    }
    struct svm_problem svmProb;
    svmProb.l = sampleNum;
    svmProb.y = Malloc(double,sampleNum);
    svmProb.x = Malloc(struct svm_node *,sampleNum);
    struct svm_node *x_space = Malloc(struct svm_node, sampleNum*(selFeature.size()+1));
    long pageSize=featureNum+1;
    long xid=0;

    //for test
    ofstream fpsvm("test_svmTrainingFeatures.txt");

    for(long sid=0; sid<sampleNum; sid++){
        long page=pageSize*sid;
        svmProb.y[sid] = train_data[page];
        //for test
        fpsvm<<train_data[page];

        svmProb.x[sid] = x_space+xid;
        for(long fid=0; fid<selFeature.size(); fid++){
            x_space[xid].index = fid;
            x_space[xid].value = train_data[page+1+selFeature[fid]];

            //for test
            fpsvm<<"\t"<<x_space[xid].value;

            ++xid;
        }
        //for test
        fpsvm<<endl;

        x_space[xid].index = -1;
        xid++;
    }
    fpsvm.close();

    if(param.svm_param.gamma == 0 && selFeature.size() > 0)
        param.svm_param.gamma = 1.0/(double)selFeature.size();

    if(param.svm_param.kernel_type == PRECOMPUTED){
        qDebug()<<"SVM ERROR: wrong kernel type, no precomputed kernel.";
        exit(1);
    }

    const char *error_msg = svm_check_parameter(&svmProb,&(param.svm_param));
    if(error_msg){
        qDebug()<<"SVM ERROR:"<<error_msg;
        exit(1);
    }
    svmModel = svm_train(&svmProb,&(param.svm_param));

    qDebug()<<"svm model training done";

    return true;
}


bool nt_selfcorrect_func::predictExisting()
{
    qDebug()<<"svm prediction: "<<ntmarkers.size()<<" to predict";
    if(svmModel==0){
        qDebug()<<"Error: cannot find svm model, need to train first";
        return false;
    }
    long fg_count=0, bg_count=0;
    for(int i=0; i<ntmarkers.size(); i++){
        double label = predictWindow((V3DLONG)ntmarkers.at(i)->x,(V3DLONG)ntmarkers.at(i)->y,(V3DLONG)ntmarkers.at(i)->z);
        if(label==VAL_FG){
            ntmarkers[i]->type=VAL_FG;
            fg_count++;
        }else if(label==VAL_BG){
            ntmarkers[i]->type=VAL_BG;
            bg_count++;
        }else{
            ntmarkers[i]->type=1;
        }
    }
    qDebug()<<"svm prediction: identified "<<fg_count<<" true trace, "<<bg_count<<" false trace.";
    return true;
}

bool nt_selfcorrect_func::correctExisting()
{
    qDebug()<<"trying to correct reconstructions";
    //identify components first
    vector<vector<MyMarker*> > components;
    for(V3DLONG i=0; i<ntmarkers.size(); i++){
        if(ntmarkers[i]->type==VAL_BG){ //break with parent if it is wrong
            ntmarkers[i]->parent=0;
            continue;
        }
        if(ntmarkers[i]->parent==0){
            continue;
        }
        if(ntmarkers[i]->parent->type==VAL_BG){ //break the wrong parent
            ntmarkers[i]->parent=0;
        }
    }
    map<MyMarker*, bool> visitMask;
    map<MyMarker*, V3DLONG> compId;
    V3DLONG tmp_id=0;
    for(V3DLONG i=0; i<ntmarkers.size(); i++){
        MyMarker* parent=ntmarkers[i]->parent;
        visitMask[ntmarkers[i]]=false;
        compId[ntmarkers[i]]=-1;
        if(parent==0 && ntmarkers[i]->type==VAL_FG){ //root of foreground
            compId[ntmarkers[i]]=tmp_id;
            visitMask[ntmarkers[i]]=true;
            tmp_id++;
        }else if(ntmarkers[i]->type==VAL_BG){ //backgrounds
            visitMask[ntmarkers[i]]=true;
        }
    }
    V3DLONG visitSum = 0;
    while(visitSum < ntmarkers.size()){
        visitSum=0;
        for(V3DLONG i=0; i<ntmarkers.size(); i++){
            if(visitMask[ntmarkers[i]]){
                visitSum++;
                continue;
            }
            MyMarker* parent=ntmarkers[i]->parent;
            if(parent==0){//this should not happen!
                qDebug()<<"Error: something unexpected happened. Please check the code and data.";
                exit(1);
            }
            if(visitMask[parent]){ //assign the parent component to the child if visited
                compId[ntmarkers[i]]=compId[parent];
                visitMask[ntmarkers[i]]=true;
            }
        }
    }
    vector<V3DLONG> compSize(tmp_id,0);
    for(V3DLONG i=0; i<ntmarkers.size(); i++){
        tmp_id = compId[ntmarkers[i]];
        if(tmp_id<0){
            continue;
        }
        compSize[tmp_id]++;
    }

    //filter and sort components by size
    multimap<V3DLONG, V3DLONG, std::greater<V3DLONG> > sizeSort;
    for(int i=0; i<compSize.size(); i++){
        sizeSort.insert(std::pair<V3DLONG,V3DLONG>(compSize[i],i) );
    }
    multimap<V3DLONG, V3DLONG, std::greater<V3DLONG> >::iterator mapiter=sizeSort.begin();
    while(mapiter!=sizeSort.end() && (mapiter->first)>param.correct_sizeThr){
        V3DLONG cid=mapiter->second;
        vector<MyMarker*> tmp_comp;
        for(V3DLONG i=0; i<ntmarkers.size(); i++){
            tmp_id = compId[ntmarkers[i]];
            if(tmp_id==cid){
                tmp_comp.push_back(ntmarkers[i]);
            }
        }
        components.push_back(tmp_comp);
        mapiter++;
    }
    //reasign component ID
    for(V3DLONG i=0; i<ntmarkers.size(); i++){
        compId[ntmarkers[i]]=-1;
    }
    for(V3DLONG cid=0; cid<components.size(); cid++){
        for(V3DLONG nid=0; nid<(components[cid]).size(); nid++){
            compId[components[cid][nid]]=cid;
        }
    }
    qDebug()<<"identified "<<components.size()<<" fragment components";
    int test_id=0; //for test

    //try to reconnect components
    map<MyMarker *, MyMarker *> extraConn;
    for(V3DLONG cid0=0; cid0<components.size(); cid0++){
        qDebug()<<"fragment: "<<cid0;
        V3DLONG ccid=-1; //the id of component it can be connected
        vector<MyMarker*> linkage; //markers linking two components
        for(V3DLONG cid1=cid0+1; cid1<components.size(); cid1++){
            //find new path via fast matching
            vector<MyMarker*> tmp_linkage;
            if(type_img==1)
                fastmarching_linker<unsigned char>(components[cid0],components[cid1],(unsigned char *)p_img1D, tmp_linkage, sz_img[0], sz_img[1], sz_img[2]);
            else if(type_img==2)
                fastmarching_linker<unsigned short>(components[cid0],components[cid1],(unsigned short *)p_img1D, tmp_linkage, sz_img[0], sz_img[1], sz_img[2]);
            else if(type_img==4)
                fastmarching_linker<float>(components[cid0],components[cid1],(float *)p_img1D, tmp_linkage, sz_img[0], sz_img[1], sz_img[2]);
            //if(tmp_linkage.size()>=linkage.size()) //no need to check if it is longer than current one
            //    continue;
            //examin the new path
            V3DLONG bg_count=0,bg_count_tmp=0;
            for(V3DLONG i = 0; i<tmp_linkage.size(); i++){
                double tmp_label=predictWindow((V3DLONG) tmp_linkage[i]->x,(V3DLONG) tmp_linkage[i]->y,(V3DLONG) tmp_linkage[i]->z);
                tmp_linkage[i]->type=tmp_label;
                if(tmp_label==VAL_BG){
                    bg_count_tmp++;
                }
                if(tmp_label==VAL_FG && bg_count_tmp>0){
                    bg_count=MAX(bg_count,bg_count_tmp);
                    bg_count_tmp==0;
                }
            }

            //for test
            QString fname_tmp="test_" + QString::number(test_id++) + ".swc";
            saveSWC_file(fname_tmp.toStdString(), tmp_linkage);

            if(bg_count<=param.correct_falseAllow){
                ccid=cid1;
                linkage.clear();
                for(V3DLONG i = 0; i<tmp_linkage.size(); i++){
                    linkage.push_back(tmp_linkage[i]);
                }
            }
        }
        if(ccid!=-1){
            qDebug()<<"merge with component "<<ccid<<"; path size: "<<linkage.size();
            //replace the first and last marker in linkage
            MyMarker * p_mm_head = linkage[0];
            double mindis=MAX_DOUBLE;
            MyMarker * p_mm_tmp;
            for(V3DLONG nid=0; nid<components[cid0].size(); nid++){
                double tmpdis=0;
                tmpdis+=(components[cid0][nid]->x-p_mm_head->x)*(components[cid0][nid]->x-p_mm_head->x);
                tmpdis+=(components[cid0][nid]->y-p_mm_head->y)*(components[cid0][nid]->y-p_mm_head->y);
                tmpdis+=(components[cid0][nid]->z-p_mm_head->z)*(components[cid0][nid]->z-p_mm_head->z);
                if(tmpdis<mindis){
                    mindis=tmpdis;
                    p_mm_tmp=components[cid0][nid];
                }
            }
            linkage[0]=p_mm_tmp;
            linkage[1]->parent=linkage[0];
            MyMarker * p_mm_tail = linkage.back();
            mindis=MAX_DOUBLE;
            for(V3DLONG nid=0; nid<components[ccid].size(); nid++){
                double tmpdis=0;
                tmpdis+=(components[ccid][nid]->x-p_mm_tail->x)*(components[ccid][nid]->x-p_mm_tail->x);
                tmpdis+=(components[ccid][nid]->y-p_mm_tail->y)*(components[ccid][nid]->y-p_mm_tail->y);
                tmpdis+=(components[ccid][nid]->z-p_mm_tail->z)*(components[ccid][nid]->z-p_mm_tail->z);
                if(tmpdis<mindis){
                    mindis=tmpdis;
                    p_mm_tmp=components[cid0][nid];
                }
            }
            extraConn[p_mm_tail->parent]=p_mm_tmp;
            linkage.pop_back();
            //push them in
            for(V3DLONG nid=0; nid<linkage.size(); nid++){
                components[cid0].push_back(linkage[nid]);
            }
            for(V3DLONG nid=0; nid<components[ccid].size(); nid++){
                components[cid0].push_back(components[ccid][nid]);
            }
            components.erase(components.begin()+ccid);
            cid0--;
        }
    }

    //reconstruct neuron tree
    //construct a graph first
    map<MyMarker *, set<MyMarker *> > neuronGraph;
    for(V3DLONG cid = 0; cid<components.size(); cid++){
        for(V3DLONG nid = 0; nid<components[cid].size(); nid++){
            MyMarker * p_mm1=components[cid][nid];
            MyMarker * p_mm2=p_mm1->parent;
            if(p_mm2!=0){
                neuronGraph[p_mm1].insert(p_mm2);
                neuronGraph[p_mm2].insert(p_mm1);
            }
        }
    }
    for(map<MyMarker *, MyMarker *>::iterator iter_extra=extraConn.begin(); iter_extra!=extraConn.end(); iter_extra++){
        neuronGraph[iter_extra->first].insert(iter_extra->second);
        neuronGraph[iter_extra->second].insert(iter_extra->first);
    }
    //reconstruct tree
    ntmarkers.clear();
    map<MyMarker *, int> mm_mask;
    for(map<MyMarker *, set<MyMarker *> >::iterator mapiter = neuronGraph.begin(); mapiter!=neuronGraph.end(); mapiter++){
        if(mm_mask[mapiter->first]==0){
            mapiter->first->parent = 0;
            mm_mask[mapiter->first]=1;
            ntmarkers.push_back(mapiter->first);
            vector<MyMarker *> mm_stack;
            mm_stack.push_back(mapiter->first);
            for(V3DLONG sid=0; sid<mm_stack.size(); sid++){
                MyMarker * p_mm_parent = mm_stack[sid];
                for(set<MyMarker *>::iterator setiter=neuronGraph[p_mm_parent].begin();
                    setiter!=neuronGraph[p_mm_parent].end(); setiter++){
                    if(mm_mask[*setiter]==0){
                        MyMarker * p_mm_child = *setiter;
                        mm_mask[p_mm_child]=1;
                        ntmarkers.push_back(p_mm_child);
                        p_mm_child->parent = p_mm_parent;
                        mm_stack.push_back(p_mm_child);
                    }
                }
            }
        }
    }

    //    //predict the whole image
    //    unsigned char * p_imgPredict=new unsigned char [sz_img[0]*sz_img[1]*sz_img[2]];
    //    memset(p_imgPredict,0,sz_img[0]*sz_img[1]*sz_img[2]);
    //    for(V3DLONG x=0; x<sz_img[0]; x++){
    //        cout<<"\r "<<x<<":"<<sz_img[0];
    //        for(V3DLONG y=0; y<sz_img[1]; y++){
    //            for(V3DLONG z=0; z<sz_img[2]; z++){
    //                if(predictWindow(x,y,z)==VAL_FG){
    //                    p_imgPredict[x+y*sz_img[1]+z*sz_img[1]*sz_img[2]]=255;
    //                }
    //            }
    //        }
    //    }

    //    //for test
    //    char fname_pred[100];
    //    sprintf(fname_pred,"test_predict.raw");
    //    saveImage(fname_pred, p_imgPredict, sz_img, 1);
}

void nt_selfcorrect_func::initParameter()
{
    param.sample_scoreThr=2;
    param.sample_radiusFactor_inter=3;
    param.sample_radiusFactor_positive=1;
    param.sample_radiusFactor_negative=4;
    param.sample_maxNumber=10000;

    param.wavelet_level=3;

    param.mrmr_feaNum=20;
    param.mrmr_method=0;
    param.mrmr_discretizeBinNumber=100;

    param.svm_param.svm_type = C_SVC;
    param.svm_param.kernel_type = RBF;
    param.svm_param.degree = 3;
    param.svm_param.gamma = 0;	// 1/num_features
    param.svm_param.coef0 = 0;
    param.svm_param.nu = 0.5;
    param.svm_param.cache_size = 100;
    param.svm_param.C = 1;
    param.svm_param.eps = 1e-3;
    param.svm_param.p = 0.1;
    param.svm_param.shrinking = 1;
    param.svm_param.probability = 0;
    param.svm_param.nr_weight = 0;
    param.svm_param.weight_label = NULL;
    param.svm_param.weight = NULL;
    //param.svm_corss_validation = 0;

    param.correct_neighborNumber=5;
    param.correct_sizeThr=2;
    param.correct_falseAllow=2;
}

void nt_selfcorrect_func::loadParameter(QString fname_param)
{

}

void nt_selfcorrect_func::saveParameter(QString fname_param)
{

}

double nt_selfcorrect_func::predictWindow(V3DLONG x, V3DLONG y, V3DLONG z)
{
    vector<float> wave;
    if(!getWindowWavelet(x,y,z,wave)){
        return 0;
    }
    if(svmNode==0){//allocate memory on first use
        svmNode = Malloc(struct svm_node, (selFeature.size()+1));
    }
    for(int i=0; i<selFeature.size(); i++){
        svmNode[i].index=i;
        svmNode[i].value=wave[selFeature[i]];
    }
    svmNode[selFeature.size()].index=-1;
    double predict_label = svm_predict(svmModel, svmNode);

    return predict_label;
}

bool nt_selfcorrect_func::getWindowWavelet(V3DLONG x, V3DLONG y, V3DLONG z, vector<float>& wave)
{
    bool b_firstRun = false;
    if(x<0||x>=sz_img[0]||y<0||y>=sz_img[1]||z<0||z>=sz_img[2]){
        qDebug()<<"error: window center of wavelet is out of boundary";
        return 0;
    }
    int level=param.wavelet_level;

    //init reusable storage memory if it is the first time to run
    if(tmp_pppp_outWave.size()!=level || tmp_ppp_window==0){
        b_firstRun = true;
        featureInfo.clear();
        winSize=1;
        tmp_pppp_outWave.resize(level);
        for(int i=level-1; i>=0; i--){
            winSize*=2;
            tmp_pppp_outWave[i]=new float *** [8];
            for(int j=0; j<8; j++){
                tmp_pppp_outWave[i][j]=new_3D_memory(winSize);
            }
        }
        tmp_ppp_window=new_3D_memory(winSize*2);
    }
    //init image
    for(int dx=-winSize; dx<winSize; dx++){
        V3DLONG cx=x+dx; //mirror for out bound points
        while(cx<0 || cx>=sz_img[0]){
            if(cx<0) cx=-(cx+1);
            if(cx>=sz_img[0]) cx=sz_img[0]*2-cx-1;
        }
        for(int dy=-winSize; dy<winSize; dy++){
            V3DLONG cy=y+dy; //mirror for out bound points
            while(cy<0 || cy>=sz_img[1]){
                if(cy<0) cy=-(cy+1);
                if(cy>=sz_img[1]) cy=sz_img[1]*2-cy-1;
            }
            for(int dz=-winSize; dz<winSize; dz++){
                V3DLONG cz=z+dz; //mirror for out bound points
                while(cz<0 || cz>=sz_img[2]){
                    if(cz<0) cz=-(cz+1);
                    if(cz>=sz_img[2]) cz=sz_img[2]*2-cz-1;
                }
                tmp_ppp_window[dz+winSize][dy+winSize][dx+winSize]=ppp_img3D[cz][cy][cx];
            }
        }
    }

    //compute wavelet
    int feaNumber=0;
    int tmp=getWavelet(tmp_ppp_window,tmp_pppp_outWave[0],winSize*2);
    if(tmp==0){
        qDebug()<<"error: failed to compute wavelet, level:0";
        return 0;
    }else{
        feaNumber+=tmp;
    }
    for(int i=1; i<level; i++){
        tmp=getWavelet(tmp_pppp_outWave[i-1][0],tmp_pppp_outWave[i],winSize/i);
        if(tmp==0){
            qDebug()<<"error: failed to compute wavelet, level:"<<i;
            return 0;
        }else{
            feaNumber+=tmp;
        }
    }

    //push to output
    feaNumber=feaNumber/8*7+8;
    wave.resize(feaNumber);
    if(b_firstRun){
        featureInfo.resize(feaNumber);
    }
    long feaInd=0;
    for(int z=0; z<2; z++){
        for(int y=0; y<2; y++){
            for(int x=0; x<2; x++){
                wave[feaInd]=tmp_pppp_outWave.at(level-1)[0][z][y][x];
                if(b_firstRun){ //record the wavelet info of feature for future analysis
                    featureInfo[feaInd].resize(5);
                    featureInfo[feaInd][0]=level;
                    featureInfo[feaInd][1]=0;
                    featureInfo[feaInd][2]=x;
                    featureInfo[feaInd][3]=y;
                    featureInfo[feaInd][4]=z;
                }

                feaInd++;
            }
        }
    }
    int cur_winSize=1;
    for(int i=level-1; i>=0; i--){
        cur_winSize*=2;
        for(int w=1; w<8; w++){
            for(int z=0; z<cur_winSize; z++){
                for(int y=0; y<cur_winSize; y++){
                    for(int x=0; x<cur_winSize; x++){
                        wave[feaInd]=tmp_pppp_outWave.at(i)[w][z][y][x];
                        if(b_firstRun){ //record the wavelet info of feature for future analysis
                            featureInfo[feaInd].resize(5);
                            featureInfo[feaInd][0]=i+1;
                            featureInfo[feaInd][1]=w;
                            featureInfo[feaInd][2]=x;
                            featureInfo[feaInd][3]=y;
                            featureInfo[feaInd][4]=z;
                        }

                        feaInd++;
                    }
                }
            }
        }
    }

//    //for test
//    cur_winSize=1;
//    V3DLONG tmp_sz[4]={1,1,1,1};
//    char tmp_filename[1000];
//    for(int i=level-1; i>=0; i--){
//        cur_winSize*=2;
//        tmp_sz[0]=tmp_sz[1]=tmp_sz[2]=cur_winSize;
//        for(int w=0; w<8; w++){
//            sprintf(tmp_filename,"test_wave_l%d_w%d.raw",i,w);
//            saveImage(tmp_filename, (unsigned char *) tmp_pppp_outWave[i][w][0][0], tmp_sz, 4);
//        }
//    }

    return true;
}

//ppp_inImage is dim*dim*dim size 3D image, dim%2==0, ppp_inImage[z][y][x]
//pppp_outWave is 8*(dim/2)*(dim/2)*(dim/2) 4D image, pppp_outWave[w][z][y][x]
//w from 0~7 are: LLL, HLL, LHL, LLH, LHH, HLH, HHL, HHH
int nt_selfcorrect_func::getWavelet(float *** ppp_inImage, float **** pppp_outWave, int dim)
{
    float L[2];
    float H[2];
    float LL[2];
    float LH[2];
    float HL[2];
    float HH[2];
    for(int z=0; z<dim/2; z++){
        for(int y=0; y<dim/2; y++){
            for(int x=0; x<dim/2; x++){
                for(int dz=0; dz<2; dz++){
                    for(int dy=0; dy<2; dy++){
                        L[dy]=(ppp_inImage[z*2+dz][y*2+dy][x*2]+ppp_inImage[z*2+dz][y*2+dy][x*2+1])/2;
                        H[dy]=(ppp_inImage[z*2+dz][y*2+dy][x*2]-ppp_inImage[z*2+dz][y*2+dy][x*2+1]);
                    }
                    LL[dz]=(L[0]+L[1])/2;
                    LH[dz]=L[0]-L[1];
                    HL[dz]=(H[0]+H[1])/2;
                    HH[dz]=H[0]-H[1];
                }
                pppp_outWave[0][z][y][x]=(LL[0]+LL[1])/2;
                pppp_outWave[1][z][y][x]=(HL[0]+HL[1])/2;
                pppp_outWave[2][z][y][x]=(LH[0]+LH[1])/2;
                pppp_outWave[3][z][y][x]=(LL[0]-LL[1]);
                pppp_outWave[4][z][y][x]=(LH[0]-LH[1]);
                pppp_outWave[5][z][y][x]=(HL[0]-HL[1]);
                pppp_outWave[6][z][y][x]=(HH[0]+HH[1])/2;
                pppp_outWave[7][z][y][x]=(HH[0]-HH[1]);
            }
        }
    }

    return dim*dim*dim;
}

void nt_selfcorrect_func::ind2sub(V3DLONG &x, V3DLONG &y, V3DLONG &z, V3DLONG ind)
{
    z=ind/(sz_img[0]*sz_img[1]);
    y=(ind-z*sz_img[0]*sz_img[1])/sz_img[0];
    x=ind-z*sz_img[0]*sz_img[1]-y*sz_img[0];
}

long nt_selfcorrect_func::sub2ind(V3DLONG x, V3DLONG y, V3DLONG z)
{
    return(z*sz_img[0]*sz_img[1]+y*sz_img[0]+x);
}

float *** new_3D_memory(int dim){
    float * data_1p=new float [dim*dim*dim];
    float *** data = new float ** [dim];
    for(int i=0; i<dim; i++){
        data[i]=new float * [dim];
        for(int j=0; j<dim; j++){
            data[i][j]=data_1p+i*dim*dim+j*dim;
        }
    }
    return data;
}

void delete_3D_memory(float ***& data, int dim){
    if(data==0){
        return;
    }
    float * data_1p=data[0][0];
    for(int i=0; i<dim; i++){
        if(data[i]){
            delete[] (data[i]);
            data[i]=0;
        }
    }
    delete [] data_1p;
    data_1p=0;
    delete [] data;
    data=0;

    return;
}