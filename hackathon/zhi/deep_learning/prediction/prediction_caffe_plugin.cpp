/* prediction_caffe_plugin.cpp
 * This is a test plugin, you can use it as a demo.
 * 2017-1-31 : by Zhi Zhou
 */
 
#include "v3d_message.h"
#include <vector>
#include "prediction_caffe_plugin.h"
#include <cmath>
#include <iostream>


#include <classification.h>
#include "../../../../released_plugins/v3d_plugins/istitch/y_imglib.h"
#include "../../AllenNeuron_postprocessing/sort_swc_IVSCC.h"
#include "../../../../released_plugins/v3d_plugins/resample_swc/resampling.h"
#include "../../../../released_plugins/v3d_plugins/mean_shift_center/mean_shift_fun.h"
#include "../../../../released_plugins/v3d_plugins/terastitcher/src/core/imagemanager/VirtualVolume.h"
#include "../../../xiaoxiaol/consensus_skeleton_2/mst_boost_prim.h"


using namespace std;
using namespace iim;
using namespace boost;

double vectorDistance(std::vector<float> v1, std::vector<float> v2)
{
  double ret = 0.0;
  for(int i = 0; i< v1.size();i++)
  {
      double dist = (v1.at(i) - v2.at(i));
      ret += dist * dist;
  }
  return ret > 0.0 ? sqrt(ret) : 0.0;
}

Q_EXPORT_PLUGIN2(prediction_caffe, prediction_caffe);
 
QStringList prediction_caffe::menulist() const
{
	return QStringList() 
        <<tr("Prediction")
        <<tr("Quality_Assess")
        <<tr("Detection")
        <<tr("Feature_Extraction")
        <<tr("Connection")
        <<tr("about");
}

QStringList prediction_caffe::funclist() const
{
	return QStringList()
        <<tr("Prediction")
        <<tr("Quality_Assess")
        <<tr("Detection")
        <<tr("Prediction_type")
        <<tr("Noise_removal")
        <<tr("3D_Axon_detection")
		<<tr("help");
}

void prediction_caffe::domenu(const QString &menu_name, V3DPluginCallback2 &callback, QWidget *parent)
{
    if (menu_name == tr("Prediction"))
	{
        string model_file = "/local1/work/caffe/models/bvlc_reference_caffenet/deploy.prototxt";
        string trained_file = "/local1/work/caffe/models/bvlc_reference_caffenet/caffenet_train_iter_2nd_450000.caffemodel";
        string mean_file = "/local1/work/caffe/data/ilsvrc12/imagenet_mean.binaryproto";
        Classifier classifier(model_file, trained_file, mean_file);


        QString m_InputfolderName = 0;
        m_InputfolderName = QFileDialog::getExistingDirectory(parent, QObject::tr("Choose the directory including all images "),
                                              QDir::currentPath(),
                                              QFileDialog::ShowDirsOnly);
        if(m_InputfolderName == 0)
            return;

        QStringList imgList = importSeriesFileList_addnumbersort(m_InputfolderName);

        Y_VIM<REAL, V3DLONG, indexed_t<V3DLONG, REAL>, LUT<V3DLONG> > vim;

        V3DLONG count=0;
        foreach (QString img_str, imgList)
        {
            V3DLONG offset[3];
            offset[0]=0; offset[1]=0; offset[2]=0;

            indexed_t<V3DLONG, REAL> idx_t(offset);

            idx_t.n = count;
            idx_t.ref_n = 0; // init with default values
            idx_t.fn_image = img_str.toStdString();
            idx_t.score = 0;

            vim.tilesList.push_back(idx_t);
            count++;
        }

        int NTILES  = vim.tilesList.size();
        std::vector<cv::Mat> imgs;
        for (V3DLONG i = 0; i <NTILES; i++)
        {
            cv::Mat img = cv::imread(vim.tilesList.at(i).fn_image.c_str());
            imgs.push_back(img);
        }
        std::vector<std::vector<float> > outputs = classifier.Predict(imgs);
        double p_num = 0;
        double n_num = 0;
        for (int j = 0; j < outputs.size(); j++)
        {
            std::vector<float> output = outputs[j];
            if(output.at(0) > output.at(1))
                n_num++;
            else
                p_num++;

        }
        double p_rate = p_num/(p_num+n_num);
        double n_rate = n_num/(p_num+n_num);

        printf("Positive rate is %.2f, and negative rate is %.2f\n",p_rate,n_rate);
        imgs.clear();
    }else if (menu_name == tr("Quality_Assess"))
    {
        v3dhandle curwin = callback.currentImageWindow();
        if (!curwin)
        {
            QMessageBox::information(0, "", "You don't have any image open in the main window.");
            return;
        }

        Image4DSimple* p4DImage = callback.getImage(curwin);

        if (!p4DImage)
        {
            QMessageBox::information(0, "", "The image pointer is invalid. Ensure your data is valid and try again!");
            return;
        }

        unsigned char* data1d = p4DImage->getRawData();
        QString imagename = callback.getImageName(curwin);

        V3DLONG N = p4DImage->getXDim();
        V3DLONG M = p4DImage->getYDim();
        V3DLONG P = p4DImage->getZDim();
        V3DLONG sc = p4DImage->getCDim();

        V3DLONG in_sz[4];
        in_sz[0] = N; in_sz[1] = M; in_sz[2] = P; in_sz[3] = sc;

        bool ok1, ok2, ok3;
        unsigned int Wx=1, Wy=1, Wz=1;

        Wx = QInputDialog::getInteger(parent, "Window X ",
                                      "Enter radius (window size is 2*radius+1):",
                                      30, 1, N, 1, &ok1);

        if(ok1)
        {
            Wy = QInputDialog::getInteger(parent, "Window Y",
                                          "Enter radius (window size is 2*radius+1):",
                                          30, 1, M, 1, &ok2);
        }
        else
            return;

        if(ok2)
        {
            Wz = QInputDialog::getInteger(parent, "Window Z",
                                          "Enter radius (window size is 2*radius+1):",
                                          25, 1, P, 1, &ok3);
        }
        else
            return;


        QString SWCfileName;
        SWCfileName = QFileDialog::getOpenFileName(0, QObject::tr("Open SWC File"),
                "",
                QObject::tr("Supported file (*.swc *.eswc)"
                    ";;Neuron structure	(*.swc)"
                    ";;Extended neuron structure (*.eswc)"
                    ));
        if(SWCfileName.isEmpty())
            return;

        NeuronTree nt = readSWC_file(SWCfileName);

        string model_file = "/local4/Data/IVSCC_test/comparison/Caffe_testing_3nd/train_package_4th/deploy.prototxt";
        string trained_file = "/local4/Data/IVSCC_test/comparison/Caffe_testing_3nd/train_package_4th/caffenet_train_iter_270000.caffemodel";
        string mean_file = "/local4/Data/IVSCC_test/comparison/Caffe_testing_3nd/train_package_4th/imagenet_mean.binaryproto";

        Classifier classifier(model_file, trained_file, mean_file);
        std::vector<cv::Mat> imgs;

        for (V3DLONG i=0;i<nt.listNeuron.size();i++)
        {
            V3DLONG tmpx = nt.listNeuron.at(i).x;
            V3DLONG tmpy = nt.listNeuron.at(i).y;
            V3DLONG tmpz = nt.listNeuron.at(i).z;

            V3DLONG xb = tmpx-1-Wx; if(xb<0) xb = 0;if(xb>=N-1) xb = N-1;
            V3DLONG xe = tmpx-1+Wx; if(xe>=N-1) xe = N-1;
            V3DLONG yb = tmpy-1-Wy; if(yb<0) yb = 0;if(yb>=M-1) yb = M-1;
            V3DLONG ye = tmpy-1+Wy; if(ye>=M-1) ye = M-1;
            V3DLONG zb = tmpz-1-Wz; if(zb<0) zb = 0;if(zb>=P-1) zb = P-1;
            V3DLONG ze = tmpz-1+Wz; if(ze>=P-1) ze = P-1;

            V3DLONG im_cropped_sz[4];
            im_cropped_sz[0] = xe - xb + 1;
            im_cropped_sz[1] = ye - yb + 1;
            im_cropped_sz[2] = 1;
            im_cropped_sz[3] = sc;

            unsigned char *im_cropped = 0;

            V3DLONG pagesz = im_cropped_sz[0]* im_cropped_sz[1]* im_cropped_sz[2]*im_cropped_sz[3];
            try {im_cropped = new unsigned char [pagesz];}
            catch(...)  {v3d_msg("cannot allocate memory for im_cropped."); return;}
            memset(im_cropped, 0, sizeof(unsigned char)*pagesz);

            for(V3DLONG iz = zb; iz <= ze; iz++)
            {
                V3DLONG offsetk = iz*M*N;
                V3DLONG j = 0;
                for(V3DLONG iy = yb; iy <= ye; iy++)
                {
                    V3DLONG offsetj = iy*N;
                    for(V3DLONG ix = xb; ix <= xe; ix++)
                    {
                        if(data1d[offsetk + offsetj + ix] >= im_cropped[j])
                            im_cropped[j] = data1d[offsetk + offsetj + ix];
                        j++;
                    }
                }
            }

            cv::Mat img(im_cropped_sz[1], im_cropped_sz[0], CV_8UC1, im_cropped);
            imgs.push_back(img);
        }

        std::vector<std::vector<float> > outputs = classifier.Predict(imgs);
        double p_num = 0;
        double n_num = 0;
        QList <ImageMarker> marklist;
        QString markerpath =  SWCfileName + QString("_fp.marker");
        for (V3DLONG j=0;j<nt.listNeuron.size();j++)
        {
            std::vector<float> output = outputs[j];
            if(output.at(0) > output.at(1))
            {
                ImageMarker S;
                S.x = nt.listNeuron.at(j).x;
                S.y = nt.listNeuron.at(j).y;
                S.z = nt.listNeuron.at(j).z;
                marklist.append(S);
                n_num++;
            }
            else
                p_num++;

        }

        writeMarker_file(markerpath.toStdString().c_str(),marklist);
        cout<<"\positive rate: "<<p_num/outputs.size()<<" and negative rate: "<<n_num/outputs.size()<<endl;
        imgs.clear();

    }
    else if (menu_name == tr("Detection"))
    {
        v3dhandle curwin = callback.currentImageWindow();
        if (!curwin)
        {
            QMessageBox::information(0, "", "You don't have any image open in the main window.");
            return;
        }

        Image4DSimple* p4DImage = callback.getImage(curwin);

        if (!p4DImage)
        {
            QMessageBox::information(0, "", "The image pointer is invalid. Ensure your data is valid and try again!");
            return;
        }

        unsigned char* data1d = p4DImage->getRawData();
        QString imagename = callback.getImageName(curwin);

        V3DLONG N = p4DImage->getXDim();
        V3DLONG M = p4DImage->getYDim();
        V3DLONG P = p4DImage->getZDim();
        V3DLONG sc = p4DImage->getCDim();

        V3DLONG in_sz[4];
        in_sz[0] = N; in_sz[1] = M; in_sz[2] = P; in_sz[3] = sc;

        string model_file = "/local4/Data/IVSCC_test/comparison/Caffe_testing_3nd/train_package_4th/deploy.prototxt";
        string trained_file = "/local4/Data/IVSCC_test/comparison/Caffe_testing_3nd/train_package_4th/caffenet_train_iter_270000.caffemodel";
        string mean_file = "/local4/Data/IVSCC_test/comparison/Caffe_testing_3nd/train_package_4th/imagenet_mean.binaryproto";

        Classifier classifier(model_file, trained_file, mean_file);
        std::vector<cv::Mat> imgs;

        bool ok;
        int Sxy =  QInputDialog::getInteger(parent, "Sample rate",
                                              "Enter sample step size:",
                                              30, 1, N, 1, &ok);
        if(!ok)
            return;

        int Wx = 30, Wy = 30, Wz = 15;
        int Sz = (int)Sxy/3;

        V3DLONG num_patches = 0;
        std::vector<std::vector<float> > outputs_overall;
        std::vector<std::vector<float> > outputs;
        for(V3DLONG iz = 0; iz < P; iz = iz+Sz)
        {
            for(V3DLONG iy = Sxy; iy < M; iy = iy+Sxy)
            {
                for(V3DLONG ix = Sxy; ix < N; ix = ix+Sxy)
                {

                    V3DLONG xb = ix-Wx; if(xb<0) xb = 0;if(xb>=N-1) xb = N-1;
                    V3DLONG xe = ix+Wx; if(xe>=N-1) xe = N-1;
                    V3DLONG yb = iy-Wy; if(yb<0) yb = 0;if(yb>=M-1) yb = M-1;
                    V3DLONG ye = iy+Wy; if(ye>=M-1) ye = M-1;
                    V3DLONG zb = iz-Wz; if(zb<0) zb = 0;if(zb>=P-1) zb = P-1;
                    V3DLONG ze = iz+Wz; if(ze>=P-1) ze = P-1;

                  //  v3d_msg(QString("%1,%2,%3,%4,%5,%6").arg(xb).arg(xe).arg(yb).arg(ye).arg(zb).arg(ze));
                    V3DLONG im_cropped_sz[4];
                    im_cropped_sz[0] = xe - xb + 1;
                    im_cropped_sz[1] = ye - yb + 1;
                    im_cropped_sz[2] = 1;
                    im_cropped_sz[3] = sc;

                    unsigned char *im_cropped = 0;

                    V3DLONG pagesz = im_cropped_sz[0]* im_cropped_sz[1]* im_cropped_sz[2]*im_cropped_sz[3];
                    try {im_cropped = new unsigned char [pagesz];}
                    catch(...)  {v3d_msg("cannot allocate memory for im_cropped."); return;}
                    memset(im_cropped, 0, sizeof(unsigned char)*pagesz);

                    for(V3DLONG iiz = zb; iiz <= ze; iiz++)
                    {
                        V3DLONG offsetk = iiz*M*N;
                        V3DLONG j = 0;
                        for(V3DLONG iiy = yb; iiy <= ye; iiy++)
                        {
                            V3DLONG offsetj = iiy*N;
                            for(V3DLONG iix = xb; iix <= xe; iix++)
                            {
                                if(data1d[offsetk + offsetj + iix] >= im_cropped[j])
                                    im_cropped[j] = data1d[offsetk + offsetj + iix];
                                j++;
                            }
                        }
                    }
                    cv::Mat img(im_cropped_sz[1], im_cropped_sz[0], CV_8UC1, im_cropped);
                    imgs.push_back(img);

                    if(num_patches >=5000)
                    {
                        outputs = classifier.Predict(imgs);
                        for(V3DLONG d = 0; d<outputs.size();d++)
                            outputs_overall.push_back(outputs[d]);
                        outputs.clear();
                        imgs.clear();
                        num_patches = 0;
                    }else
                        num_patches++;
                }
            }
        }

        if(imgs.size()>0)
        {
            outputs = classifier.Predict(imgs);
            for(V3DLONG d = 0; d<outputs.size();d++)
                outputs_overall.push_back(outputs[d]);
        }

        QList <ImageMarker> marklist;
        QString markerpath =  imagename + QString("_%1.marker").arg(Sxy);

        V3DLONG d = 0;
        for(V3DLONG iz = 0; iz < P; iz = iz+Sz)
        {
            for(V3DLONG iy = Sxy; iy < M; iy = iy+Sxy)
            {
                for(V3DLONG ix = Sxy; ix < N; ix = ix+Sxy)
                {
                    std::vector<float> output = outputs_overall[d];
                    if(output.at(1) > output.at(0))
                    {
                        ImageMarker S;
                        S.x = ix;
                        S.y = iy;
                        S.z = iz;
                        marklist.append(S);
                    }
                    d++;
                }
            }
        }

        writeMarker_file(markerpath.toStdString().c_str(),marklist);
        outputs_overall.clear();
        imgs.clear();
    }else if (menu_name == tr("Feature_Extraction"))
    {
        v3dhandle curwin = callback.currentImageWindow();
        if (!curwin)
        {
            QMessageBox::information(0, "", "You don't have any image open in the main window.");
            return;
        }

        Image4DSimple* p4DImage = callback.getImage(curwin);

        if (!p4DImage)
        {
            QMessageBox::information(0, "", "The image pointer is invalid. Ensure your data is valid and try again!");
            return;
        }

        unsigned char* data1d = p4DImage->getRawData();
        QString imagename = callback.getImageName(curwin);

        V3DLONG N = p4DImage->getXDim();
        V3DLONG M = p4DImage->getYDim();
        V3DLONG P = p4DImage->getZDim();
        V3DLONG sc = p4DImage->getCDim();

        V3DLONG in_sz[4];
        in_sz[0] = N; in_sz[1] = M; in_sz[2] = P; in_sz[3] = sc;

        bool ok1, ok2, ok3;
        unsigned int Wx=1, Wy=1, Wz=1;

        Wx = QInputDialog::getInteger(parent, "Window X ",
                                      "Enter radius (window size is 2*radius+1):",
                                      30, 1, N, 1, &ok1);

        if(ok1)
        {
            Wy = QInputDialog::getInteger(parent, "Window Y",
                                          "Enter radius (window size is 2*radius+1):",
                                          30, 1, M, 1, &ok2);
        }
        else
            return;

        if(ok2)
        {
            Wz = QInputDialog::getInteger(parent, "Window Z",
                                          "Enter radius (window size is 2*radius+1):",
                                          25, 1, P, 1, &ok3);
        }
        else
            return;


        QString SWCfileName;
        SWCfileName = QFileDialog::getOpenFileName(0, QObject::tr("Open SWC File"),
                "",
                QObject::tr("Supported file (*.swc *.eswc)"
                    ";;Neuron structure	(*.swc)"
                    ";;Extended neuron structure (*.eswc)"
                    ));
        if(SWCfileName.isEmpty())
            return;

        NeuronTree nt = readSWC_file(SWCfileName);
        string model_file = "/local1/work/caffe/examples/siamese/mnist_siamese.prototxt";
        string trained_file = "/local1/work/caffe/examples/siamese/full_siamese_iter_450000.caffemodel";
        Classifier classifier(model_file, trained_file,"");
        std::vector<cv::Mat> imgs;
        for (V3DLONG i=0;i<nt.listNeuron.size();i++)
        {
            V3DLONG tmpx = nt.listNeuron.at(i).x;
            V3DLONG tmpy = nt.listNeuron.at(i).y;
            V3DLONG tmpz = nt.listNeuron.at(i).z;

            V3DLONG xb = tmpx-1-Wx; if(xb<0) xb = 0;if(xb>=N-1) xb = N-1;
            V3DLONG xe = tmpx-1+Wx; if(xe>=N-1) xe = N-1;
            V3DLONG yb = tmpy-1-Wy; if(yb<0) yb = 0;if(yb>=M-1) yb = M-1;
            V3DLONG ye = tmpy-1+Wy; if(ye>=M-1) ye = M-1;
            V3DLONG zb = tmpz-1-Wz; if(zb<0) zb = 0;if(zb>=P-1) zb = P-1;
            V3DLONG ze = tmpz-1+Wz; if(ze>=P-1) ze = P-1;

            V3DLONG im_cropped_sz[4];
            im_cropped_sz[0] = xe - xb + 1;
            im_cropped_sz[1] = ye - yb + 1;
            im_cropped_sz[2] = 1;
            im_cropped_sz[3] = sc;

            unsigned char *im_cropped = 0;

            V3DLONG pagesz = im_cropped_sz[0]* im_cropped_sz[1]* im_cropped_sz[2]*im_cropped_sz[3];
            try {im_cropped = new unsigned char [pagesz];}
            catch(...)  {v3d_msg("cannot allocate memory for im_cropped."); return;}
            memset(im_cropped, 0, sizeof(unsigned char)*pagesz);

            for(V3DLONG iz = zb; iz <= ze; iz++)
            {
                V3DLONG offsetk = iz*M*N;
                V3DLONG j = 0;
                for(V3DLONG iy = yb; iy <= ye; iy++)
                {
                    V3DLONG offsetj = iy*N;
                    for(V3DLONG ix = xb; ix <= xe; ix++)
                    {
                        if(data1d[offsetk + offsetj + ix] >= im_cropped[j])
                            im_cropped[j] = data1d[offsetk + offsetj + ix];
                        j++;
                    }
                }
            }

            cv::Mat img(im_cropped_sz[1], im_cropped_sz[0], CV_8UC1, im_cropped);
            imgs.push_back(img);
        }

        std::vector<std::vector<float> > outputs = classifier.extractFeature_siamese(imgs);
        UndirectedGraph g(nt.listNeuron.size());
        for (int i=0;i<nt.listNeuron.size()-1;i++)
        {
            for (int j=i+1;j<nt.listNeuron.size();j++)
            {
                V3DLONG x1 = nt.listNeuron.at(i).x;
                V3DLONG y1 = nt.listNeuron.at(i).y;
                V3DLONG z1 = nt.listNeuron.at(i).z;
                V3DLONG x2 = nt.listNeuron.at(j).x;
                V3DLONG y2 = nt.listNeuron.at(j).y;
                V3DLONG z2 = nt.listNeuron.at(j).z;
                double dis = sqrt(pow2(x1-x2) + pow2(y1-y2) + pow2(z1-z2));

                EdgeQuery edgeq = edge(i, j, *&g);
                if (!edgeq.second && i!=j)
                {
                    double Vedge;
                    std::vector<float> v1 = outputs[i];
                    std::vector<float> v2 = outputs[j];
                    Vedge = vectorDistance(v1, v2)*dis;

                    add_edge(i, j, LastVoted(i, Weight(Vedge)), *&g);
                }
            }
        }

        vector < graph_traits < UndirectedGraph >::vertex_descriptor > p(num_vertices(*&g));
        prim_minimum_spanning_tree(*&g, &p[0]);

        NeuronTree marker_MST;
        QList <NeuronSWC> listNeuron;
        QHash <int, int>  hashNeuron;
        listNeuron.clear();
        hashNeuron.clear();

        for (std::size_t i = 0; i != p.size(); ++i)
        {
            NeuronSWC S;
            int pn;
            if(p[i] == i)
                pn = -1;
            else
                pn = p[i] + 1;

            S.n 	= i+1;
            S.type 	= 7;
            S.x 	= nt.listNeuron.at(i).x;
            S.y 	= nt.listNeuron.at(i).y;
            S.z 	= nt.listNeuron.at(i).z;;
            S.r 	= 1;
            S.pn 	= pn;
            listNeuron.append(S);
            hashNeuron.insert(S.n, listNeuron.size()-1);
        }

        marker_MST.n = -1;
        marker_MST.on = true;
        marker_MST.listNeuron = listNeuron;
        marker_MST.hashNeuron = hashNeuron;


        for (int i=1;i<marker_MST.listNeuron.size()-1;i++)
        {
            if(marker_MST.listNeuron.at(i).parent>0)
            {
                V3DLONG x1 = marker_MST.listNeuron.at(i).x;
                V3DLONG y1 = marker_MST.listNeuron.at(i).y;
                V3DLONG z1 = marker_MST.listNeuron.at(i).z;
                V3DLONG x2 = marker_MST.listNeuron.at(marker_MST.listNeuron.at(i).parent-1).x;
                V3DLONG y2 = marker_MST.listNeuron.at(marker_MST.listNeuron.at(i).parent-1).y;
                V3DLONG z2 = marker_MST.listNeuron.at(marker_MST.listNeuron.at(i).parent-1).z;
                double dis = sqrt(pow2(x1-x2) + pow2(y1-y2) + 4*pow2(z1-z2));
                if(dis>60)
                    marker_MST.listNeuron[i].parent = -1;
            }
        }

        QString outfilename = SWCfileName + "_connected_60_z.swc";
        if (outfilename.startsWith("http", Qt::CaseInsensitive))
        {
            QFileInfo ii(outfilename);
            outfilename = QDir::home().absolutePath() + "/" + ii.fileName();
        }
        writeSWC_file(outfilename,marker_MST);
        v3d_msg(QString("The output file is [%1]").arg(outfilename));
        return;
    }else if (menu_name == tr("Connection"))
    {
        QString SWCfileName;
        SWCfileName = QFileDialog::getOpenFileName(0, QObject::tr("Open SWC File"),
                                                   "",
                                                   QObject::tr("Supported file (*.swc *.eswc)"
                                                               ";;Neuron structure	(*.swc)"
                                                               ";;Extended neuron structure (*.eswc)"
                                                               ));
        if(SWCfileName.isEmpty())
            return;
        double disTh, angTh;
        bool ok1, ok2;

        disTh = QInputDialog::getDouble(parent, "Distance ",
                                        "Distance:",
                                        120.0, 1.0, 200.0, 1.0, &ok1);

        if(ok1)
        {
            angTh = QInputDialog::getDouble(parent, "Angle",
                                          "Angle:",
                                            120.0, 1.0, 180.0, 1.0, &ok2);
        }
        else
            return;

        if(!ok2)
            return;

        NeuronTree nt = readSWC_file(SWCfileName);
        NeuronTree nt_pruned = pruneswc(nt, 2);
        NeuronTree nt_pruned_rs = resample(nt_pruned, 10);


        QString outfilename = SWCfileName + "_connected_tips.swc";

        QList<NeuronSWC> newNeuron;
        connect_swc(nt_pruned_rs,newNeuron,disTh,angTh);
        export_list2file(newNeuron, outfilename, SWCfileName);



    }
    else
	{
		v3d_msg(tr("This is a test plugin, you can use it as a demo.. "
			"Developed by Zhi Zhou, 2017-1-31"));
	}
}

bool prediction_caffe::dofunc(const QString & func_name, const V3DPluginArgList & input, V3DPluginArgList & output, V3DPluginCallback2 & callback,  QWidget * parent)
{
    vector<char*> infiles, paras, outfiles;
    if(input.size() >= 1) infiles = *((vector<char*> *)input.at(0).p);
    if(input.size() >= 2) paras = *((vector<char*> *)input.at(1).p);
    if(output.size() >= 1) outfiles = *((vector<char*> *)output.at(0).p);

    if (func_name == tr("Prediction"))
	{
        cout<<"Welcome to Caffe prediction plugin"<<endl;
        if(infiles.empty())
        {
            cerr<<"Need input image folder"<<endl;
            return false;
        }
        QString  m_InputfolderName =  infiles[0];
        int k=0;

        QString model_file = paras.empty() ? "" : paras[k]; if(model_file == "NULL") model_file = ""; k++;
        if(model_file.isEmpty())
        {
            cerr<<"Need a model_file"<<endl;
            return false;
        }

        QString trained_file = paras.empty() ? "" : paras[k]; if(trained_file == "NULL") trained_file = ""; k++;
        if(trained_file.isEmpty())
        {
            cerr<<"Need a trained_file"<<endl;
            return false;
        }

        QString mean_file = paras.empty() ? "" : paras[k]; if(mean_file == "NULL") mean_file = ""; k++;
        if(mean_file.isEmpty())
        {
            cerr<<"Need a mean_file"<<endl;
            return false;
        }

        cout<<"inimg_file = "<<m_InputfolderName.toStdString().c_str()<<endl;
        cout<<"model_file = "<<model_file.toStdString().c_str()<<endl;
        cout<<"trained_file = "<<trained_file.toStdString().c_str()<<endl;
        cout<<"mean_file = "<<mean_file.toStdString().c_str()<<endl;

        Classifier classifier(model_file.toStdString(), trained_file.toStdString(), mean_file.toStdString());

        QStringList imgList = importSeriesFileList_addnumbersort(m_InputfolderName);

        Y_VIM<REAL, V3DLONG, indexed_t<V3DLONG, REAL>, LUT<V3DLONG> > vim;

        V3DLONG count=0;
        foreach (QString img_str, imgList)
        {
            V3DLONG offset[3];
            offset[0]=0; offset[1]=0; offset[2]=0;

            indexed_t<V3DLONG, REAL> idx_t(offset);

            idx_t.n = count;
            idx_t.ref_n = 0; // init with default values
            idx_t.fn_image = img_str.toStdString();
            idx_t.score = 0;

            vim.tilesList.push_back(idx_t);
            count++;
        }

        int NTILES  = vim.tilesList.size();
        std::vector<cv::Mat> imgs;
        for (V3DLONG i = 0; i <NTILES; i++)
        {
            cv::Mat img = cv::imread(vim.tilesList.at(i).fn_image.c_str(),-1);
            imgs.push_back(img);
        }
        std::vector<std::vector<float> > outputs = classifier.Predict(imgs);

        double p_num = 0;
        double n_num = 0;
        for (int j = 0; j < outputs.size(); j++)
        {
            std::vector<float> output = outputs[j];
            if(output.at(0) > output.at(1))
                n_num++;
            else
                p_num++;

        }

        cout<<"\positive rate: "<<p_num/outputs.size()<<" and negative rate: "<<n_num/outputs.size()<<endl;

        if(!outfiles.empty())
        {
            QString  outputfile =  outfiles[0];
            QFile saveTextFile;
            saveTextFile.setFileName(outputfile);
            if (!saveTextFile.isOpen()){
                if (!saveTextFile.open(QIODevice::Text|QIODevice::Append  )){
                    qDebug()<<"unable to save file!";
                    return false;}     }
            QTextStream outputStream;
            outputStream.setDevice(&saveTextFile);
            outputStream<< m_InputfolderName<<"\t"<< NTILES<<"\t"<<p_num<<"\t"<< n_num<<"\n";
            saveTextFile.close();
        }
        imgs.clear();
        return true;
	}
    else if (func_name == tr("Quality_Assess"))
	{
        cout<<"Welcome to Caffe quality assessment plugin"<<endl;
        if(infiles.empty())
        {
            cerr<<"Need input image file"<<endl;
            return false;
        }
        QString  inimg_file =  infiles[0];
        int k=0;

        QString SWCfileName = paras.empty() ? "" : paras[k]; if(SWCfileName == "NULL") SWCfileName = ""; k++;
        if(SWCfileName.isEmpty())
        {
            cerr<<"Need a swc file"<<endl;
            return false;
        }

        QString model_file = paras.empty() ? "" : paras[k]; if(model_file == "NULL") model_file = ""; k++;
        if(model_file.isEmpty())
        {
            cerr<<"Need a model_file"<<endl;
            return false;
        }

        QString trained_file = paras.empty() ? "" : paras[k]; if(trained_file == "NULL") trained_file = ""; k++;
        if(trained_file.isEmpty())
        {
            cerr<<"Need a trained_file"<<endl;
            return false;
        }

        QString mean_file = paras.empty() ? "" : paras[k]; if(mean_file == "NULL") mean_file = ""; k++;
        if(mean_file.isEmpty())
        {
            cerr<<"Need a mean_file"<<endl;
            return false;
        }

        cout<<"inimg_file = "<<inimg_file.toStdString().c_str()<<endl;
        cout<<"inswc_file = "<<SWCfileName.toStdString().c_str()<<endl;
        cout<<"model_file = "<<model_file.toStdString().c_str()<<endl;
        cout<<"trained_file = "<<trained_file.toStdString().c_str()<<endl;
        cout<<"mean_file = "<<mean_file.toStdString().c_str()<<endl;

        unsigned char * data1d = 0;
        V3DLONG N,M,P;
        if(inimg_file.endsWith(".raw",Qt::CaseSensitive) || inimg_file.endsWith(".v3draw",Qt::CaseSensitive))
        {
            V3DLONG in_sz[4];
            int datatype;
            if(!simple_loadimage_wrapper(callback, inimg_file.toStdString().c_str(), data1d, in_sz, datatype))
            {
                cerr<<"load image "<<inimg_file.toStdString().c_str()<<" error!"<<endl;
                return false;
            }
            N = in_sz[0];
            M = in_sz[1];
            P = in_sz[2];
        }else
        {
            VirtualVolume* aVolume = VirtualVolume::instance(inimg_file.toStdString().c_str());
            N = aVolume->getDIM_H();
            M = aVolume->getDIM_V();
            P = aVolume->getDIM_D();
        }

        NeuronTree nt = readSWC_file(SWCfileName);
        unsigned int Wx=30, Wy=30, Wz=15;
        Classifier classifier(model_file.toStdString(), trained_file.toStdString(), mean_file.toStdString());
        std::vector<cv::Mat> imgs;

        for (V3DLONG i=0;i<nt.listNeuron.size();i++)
        {
            V3DLONG tmpx = nt.listNeuron.at(i).x;
            V3DLONG tmpy = nt.listNeuron.at(i).y;
            V3DLONG tmpz = nt.listNeuron.at(i).z;

            V3DLONG xb = tmpx-1-Wx; if(xb<0) xb = 0;if(xb>=N-1) xb = N-1;
            V3DLONG xe = tmpx-1+Wx; if(xe>=N-1) xe = N-1;
            V3DLONG yb = tmpy-1-Wy; if(yb<0) yb = 0;if(yb>=M-1) yb = M-1;
            V3DLONG ye = tmpy-1+Wy; if(ye>=M-1) ye = M-1;
            V3DLONG zb = tmpz-1-Wz; if(zb<0) zb = 0;if(zb>=P-1) zb = P-1;
            V3DLONG ze = tmpz-1+Wz; if(ze>=P-1) ze = P-1;

            unsigned char *im_cropped = 0;
            V3DLONG im_cropped_sz[4];
            im_cropped_sz[0] = xe - xb + 1;
            im_cropped_sz[1] = ye - yb + 1;
            im_cropped_sz[2] = 1;
            V3DLONG pagesz = im_cropped_sz[0]* im_cropped_sz[1]* im_cropped_sz[2];
            try {im_cropped = new unsigned char [pagesz];}
            catch(...)  {v3d_msg("cannot allocate memory for im_cropped."); return false;}
            memset(im_cropped, 0, sizeof(unsigned char)*pagesz);

            if(inimg_file.endsWith(".raw",Qt::CaseSensitive) || inimg_file.endsWith(".v3draw",Qt::CaseSensitive))
            {
                for(V3DLONG iz = zb; iz <= ze; iz++)
                {
                    V3DLONG offsetk = iz*M*N;
                    V3DLONG j = 0;
                    for(V3DLONG iy = yb; iy <= ye; iy++)
                    {
                        V3DLONG offsetj = iy*N;
                        for(V3DLONG ix = xb; ix <= xe; ix++)
                        {
                            if(data1d[offsetk + offsetj + ix] >= im_cropped[j])
                                im_cropped[j] = data1d[offsetk + offsetj + ix];
                            j++;
                        }
                    }
                }
            }else
            {
                unsigned char *im_cropped_3D = 0;
                VirtualVolume* aVolume = VirtualVolume::instance(inimg_file.toStdString().c_str());
                im_cropped_3D = aVolume->loadSubvolume_to_UINT8(yb,ye+1,xb,xe+1,zb,ze+1);
                for(V3DLONG iz = 0; iz <= ze-zb; iz++)
                {
                    V3DLONG offsetk = iz*im_cropped_sz[0]*im_cropped_sz[1];
                    V3DLONG j = 0;
                    for(V3DLONG iy = 0; iy <= ye -yb; iy++)
                    {
                        V3DLONG offsetj = iy*im_cropped_sz[0];
                        for(V3DLONG ix = 0; ix <= xe-xb; ix++)
                        {
                            if(im_cropped_3D[offsetk + offsetj + ix] >= im_cropped[j])
                                im_cropped[j] = im_cropped_3D[offsetk + offsetj + ix];
                            j++;
                        }
                    }
                }
                if(im_cropped_3D) {delete []im_cropped_3D; im_cropped_3D=0;}
            }

            cv::Mat img(im_cropped_sz[1], im_cropped_sz[0], CV_8UC1, im_cropped);
            imgs.push_back(img);
        }

        std::vector<std::vector<float> > outputs = classifier.Predict(imgs);
        double p_num = 0;
        double n_num = 0;
        QList <ImageMarker> marklist;
        QString markerpath =  SWCfileName + QString("_fp.marker");
        for (V3DLONG j=0;j<nt.listNeuron.size();j++)
        {
            std::vector<float> output = outputs[j];
            if(output.at(0) > output.at(1))
            {
                ImageMarker S;
                S.x = nt.listNeuron.at(j).x;
                S.y = nt.listNeuron.at(j).y;
                S.z = nt.listNeuron.at(j).z;
                marklist.append(S);
                n_num++;
            }
            else
                p_num++;

        }
        writeMarker_file(markerpath.toStdString().c_str(),marklist);
        cout<<"\positive rate: "<<p_num/outputs.size()<<" and negative rate: "<<n_num/outputs.size()<<endl;
        imgs.clear();
        if(data1d) {delete []data1d; data1d = 0;}
        return true;
    }else if (func_name == tr("Detection"))
    {
        cout<<"Welcome to Caffe signal detection plugin"<<endl;
        if(infiles.empty())
        {
            cerr<<"Need input image file"<<endl;
            return false;
        }
        QString  inimg_file =  infiles[0];
        int k=0;

        QString model_file = paras.empty() ? "" : paras[k]; if(model_file == "NULL") model_file = ""; k++;
        if(model_file.isEmpty())
        {
            cerr<<"Need a model_file"<<endl;
            return false;
        }

        QString trained_file = paras.empty() ? "" : paras[k]; if(trained_file == "NULL") trained_file = ""; k++;
        if(trained_file.isEmpty())
        {
            cerr<<"Need a trained_file"<<endl;
            return false;
        }

        QString mean_file = paras.empty() ? "" : paras[k]; if(mean_file == "NULL") mean_file = ""; k++;
        if(mean_file.isEmpty())
        {
            cerr<<"Need a mean_file"<<endl;
            return false;
        }

        int Sxy = paras.empty() ? 10 : atoi(paras[k]);


        cout<<"inimg_file = "<<inimg_file.toStdString().c_str()<<endl;
        cout<<"model_file = "<<model_file.toStdString().c_str()<<endl;
        cout<<"trained_file = "<<trained_file.toStdString().c_str()<<endl;
        cout<<"mean_file = "<<mean_file.toStdString().c_str()<<endl;
        cout<<"sample_size = "<<Sxy<<endl;

        Classifier classifier(model_file.toStdString(), trained_file.toStdString(), mean_file.toStdString());
        std::vector<cv::Mat> imgs;

        unsigned char * data1d = 0;
        V3DLONG in_sz[4];

        int datatype;
        if(!simple_loadimage_wrapper(callback, inimg_file.toStdString().c_str(), data1d, in_sz, datatype))
        {
            cerr<<"load image "<<inimg_file.toStdString().c_str()<<" error!"<<endl;
            return false;
        }

        V3DLONG N = in_sz[0];
        V3DLONG M = in_sz[1];
        V3DLONG P = in_sz[2];
        V3DLONG sc = in_sz[3];

        int Wx = 30, Wy = 30, Wz = 15;
        int Sz = (int)Sxy/3;

        V3DLONG num_patches = 0;
        std::vector<std::vector<float> > outputs_overall;
        std::vector<std::vector<float> > outputs;
        for(V3DLONG iz = 0; iz < P; iz = iz+Sz)
        {
            for(V3DLONG iy = Sxy; iy < M; iy = iy+Sxy)
            {
                for(V3DLONG ix = Sxy; ix < N; ix = ix+Sxy)
                {

                    V3DLONG xb = ix-Wx; if(xb<0) xb = 0;if(xb>=N-1) xb = N-1;
                    V3DLONG xe = ix+Wx; if(xe>=N-1) xe = N-1;
                    V3DLONG yb = iy-Wy; if(yb<0) yb = 0;if(yb>=M-1) yb = M-1;
                    V3DLONG ye = iy+Wy; if(ye>=M-1) ye = M-1;
                    V3DLONG zb = iz-Wz; if(zb<0) zb = 0;if(zb>=P-1) zb = P-1;
                    V3DLONG ze = iz+Wz; if(ze>=P-1) ze = P-1;

                    V3DLONG im_cropped_sz[4];
                    im_cropped_sz[0] = xe - xb + 1;
                    im_cropped_sz[1] = ye - yb + 1;
                    im_cropped_sz[2] = 1;
                    im_cropped_sz[3] = sc;

                    unsigned char *im_cropped = 0;

                    V3DLONG pagesz = im_cropped_sz[0]* im_cropped_sz[1]* im_cropped_sz[2]*im_cropped_sz[3];
                    try {im_cropped = new unsigned char [pagesz];}
                    catch(...)  {v3d_msg("cannot allocate memory for im_cropped."); return false;}
                    memset(im_cropped, 0, sizeof(unsigned char)*pagesz);

                    for(V3DLONG iiz = zb; iiz <= ze; iiz++)
                    {
                        V3DLONG offsetk = iiz*M*N;
                        V3DLONG j = 0;
                        for(V3DLONG iiy = yb; iiy <= ye; iiy++)
                        {
                            V3DLONG offsetj = iiy*N;
                            for(V3DLONG iix = xb; iix <= xe; iix++)
                            {
                                if(data1d[offsetk + offsetj + iix] >= im_cropped[j])
                                    im_cropped[j] = data1d[offsetk + offsetj + iix];
                                j++;
                            }
                        }
                    }
                    cv::Mat img(im_cropped_sz[1], im_cropped_sz[0], CV_8UC1, im_cropped);
                    imgs.push_back(img);

                    if(num_patches >=5000)
                    {
                        outputs = classifier.Predict(imgs);
                        for(V3DLONG d = 0; d<outputs.size();d++)
                            outputs_overall.push_back(outputs[d]);
                        outputs.clear();
                        imgs.clear();
                        num_patches = 0;
                    }else
                        num_patches++;
                }
            }
        }

        if(imgs.size()>0)
        {
            outputs = classifier.Predict(imgs);
            for(V3DLONG d = 0; d<outputs.size();d++)
                outputs_overall.push_back(outputs[d]);
        }

        QList <ImageMarker> marklist;
        QString markerpath =  inimg_file + QString("_%1.marker").arg(Sxy);

        V3DLONG d = 0;
        for(V3DLONG iz = 0; iz < P; iz = iz+Sz)
        {
            for(V3DLONG iy = Sxy; iy < M; iy = iy+Sxy)
            {
                for(V3DLONG ix = Sxy; ix < N; ix = ix+Sxy)
                {
                    std::vector<float> output = outputs_overall[d];
                    if(output.at(1) > output.at(0))
                    {
                        ImageMarker S;
                        S.x = ix;
                        S.y = iy;
                        S.z = iz;
                        marklist.append(S);
                    }
                    d++;
                }
            }
        }
        writeMarker_file(markerpath.toStdString().c_str(),marklist);
        outputs_overall.clear();
        imgs.clear();
        if(data1d) {delete []data1d; data1d = 0;}
        return true;
    }
    else if(func_name == tr("Prediction_type"))
    {
        cout<<"Welcome to Caffe signal detection plugin"<<endl;
        if(infiles.empty())
        {
            cerr<<"Need input swc file"<<endl;
            return false;
        }
        QString  inswc_file =  infiles[0];
        int k=0;

        QString marker_file = paras.empty() ? "" : paras[k]; if(marker_file == "NULL") marker_file = ""; k++;
        if(marker_file.isEmpty())
        {
            cerr<<"Need a marker file"<<endl;
            return false;
        }

        cout<<"inswc_file = "<<inswc_file.toStdString().c_str()<<endl;
        cout<<"marker_file = "<<marker_file.toStdString().c_str()<<endl;

        NeuronTree nt = readSWC_file(inswc_file);
        QList <ImageMarker> marklist =  readMarker_file(marker_file);

        double apical_total = 0,  apical_fn = 0, dendrite_total = 0, dendrite_fn = 0;
        double axon_total = 0,axon_fn = 0;

        NeuronTree nt_prunned;
        QList <NeuronSWC> listNeuron;
        QHash <int, int>  hashNeuron;
        listNeuron.clear();
        hashNeuron.clear();
        NeuronSWC S;

        for (V3DLONG i=0;i<nt.listNeuron.size();i++)
        {
            bool flag = false;
            if(nt.listNeuron.at(i).type == 2)
                axon_total++;
            else if (nt.listNeuron.at(i).type == 3)
                dendrite_total++;
            else if (nt.listNeuron.at(i).type == 4)
                apical_total++;
            for(V3DLONG j=0; j<marklist.size();j++)
            {
                double dis = sqrt(pow2(nt.listNeuron.at(i).x - marklist.at(j).x) + pow2(nt.listNeuron.at(i).y - marklist.at(j).y) + pow2(nt.listNeuron.at(i).z - marklist.at(j).z));
                if(dis < 1.0)
                {
                    if(nt.listNeuron.at(i).type == 2)
                        axon_fn++;
                    else if (nt.listNeuron.at(i).type == 3)
                        dendrite_fn++;
                    else if (nt.listNeuron.at(i).type == 4)
                        apical_fn++;
                    flag = true;
                    break;
                }
            }
            if(!flag)
            {
                NeuronSWC curr = nt.listNeuron.at(i);
                S.n 	= curr.n;
                S.type 	= curr.type;
                S.x 	= curr.x;
                S.y 	= curr.y;
                S.z 	= curr.z;
                S.r 	= curr.r;
                S.pn 	= curr.pn;
                listNeuron.append(S);
                hashNeuron.insert(S.n, listNeuron.size()-1);
            }
        }

        nt_prunned.n = -1;
        nt_prunned.on = true;
        nt_prunned.listNeuron = listNeuron;
        nt_prunned.hashNeuron = hashNeuron;

        cout<<"\naxon_total: "<<axon_total<<" ,axon_fn: "<<axon_fn<<endl;
        cout<<"dendrite_total: "<<dendrite_total<<" ,dendrite_fn: "<<dendrite_fn<<endl;
        cout<<"apical_total: "<<apical_total<<" ,apical_fn: "<<apical_fn<<endl;

        NeuronTree nt_prunned_sort = SortSWC_pipeline(nt_prunned.listNeuron,VOID, 0);
        QList<NeuronSWC> newNeuron_connected;
        double angthr=cos((180-60)/180*M_PI);
        QString  swc_processed = inswc_file + "_processed.swc";

        connectall(&nt_prunned_sort, newNeuron_connected, 1, 1, 1, angthr, 100, 1, false, -1);

        if(!export_list2file(newNeuron_connected, swc_processed,inswc_file)){
            qDebug()<<"error: Cannot open file "<<swc_processed<<" for writing!"<<endl;
        }


        NeuronTree nt_connected = readSWC_file(swc_processed);
        NeuronTree nt_prunned_sort_rs = resample(nt_connected, 10);

        double apical_fn_updated = 0, dendrite_fn_updated = 0, axon_fn_updated = 0;

        QList <ImageMarker> marklist_process;
        for(V3DLONG i=0; i<marklist.size();i++)
        {
            bool flag = false;
            for (V3DLONG j=0;j<nt_prunned_sort_rs.listNeuron.size();j++)
            {
                NeuronSWC curr = nt_prunned_sort_rs.listNeuron.at(j);
                double dis = sqrt(pow2(curr.x - marklist.at(i).x) + pow2(curr.y - marklist.at(i).y) + pow2(curr.z - marklist.at(i).z));
                if(dis < 10.0)
                {
                    flag = true;
                    break;
                }
            }
            if(!flag)
            {
                for (V3DLONG j=0;j<nt.listNeuron.size();j++)
                {
                    if(nt.listNeuron.at(j).x ==marklist.at(i).x && nt.listNeuron.at(j).y == marklist.at(i).y && nt.listNeuron.at(j).z ==marklist.at(i).z)
                    {
                        if(nt.listNeuron.at(j).type == 2)
                            axon_fn_updated++;
                        else if (nt.listNeuron.at(j).type == 3)
                            dendrite_fn_updated++;
                        else if (nt.listNeuron.at(j).type == 4)
                            apical_fn_updated++;
                        break;
                    }
                }
                marklist_process.push_back(marklist.at(i));
            }
        }

        QString marker_file_process = marker_file + "_processed.marker";
        writeMarker_file(marker_file_process,marklist_process);

        if(!outfiles.empty())
        {
            QString  outputfile =  outfiles[0];
            QFile saveTextFile;
            saveTextFile.setFileName(outputfile);
            if (!saveTextFile.isOpen()){
                if (!saveTextFile.open(QIODevice::Text|QIODevice::Append  )){
                    qDebug()<<"unable to save file!";
                    return false;}     }
            QTextStream outputStream;
            outputStream.setDevice(&saveTextFile);
            outputStream<< inswc_file.toStdString().c_str()<<"\t"<<axon_total<<"\t"<< axon_fn<<"\t"<< axon_fn_updated<<"\t"<<dendrite_total<<"\t"<< dendrite_fn<<"\t"<< dendrite_fn_updated<<"\t"<<apical_total<<"\t"<< apical_fn<<"\t"<< apical_fn_updated<<"\n";
            saveTextFile.close();
        }

    }
    else if(func_name == tr("Noise_removal"))
    {
        cout<<"Welcome to Caffe noise removal plugin"<<endl;
        if(infiles.empty())
        {
            cerr<<"Need input image file"<<endl;
            return false;
        }
        QString  inimg_file =  infiles[0];
        int k=0;

        QString SWCfileName = paras.empty() ? "" : paras[k]; if(SWCfileName == "NULL") SWCfileName = ""; k++;
        if(SWCfileName.isEmpty())
        {
            cerr<<"Need a swc file"<<endl;
            return false;
        }

        QString model_file = paras.empty() ? "" : paras[k]; if(model_file == "NULL") model_file = ""; k++;
        if(model_file.isEmpty())
        {
            cerr<<"Need a model_file"<<endl;
            return false;
        }

        QString trained_file = paras.empty() ? "" : paras[k]; if(trained_file == "NULL") trained_file = ""; k++;
        if(trained_file.isEmpty())
        {
            cerr<<"Need a trained_file"<<endl;
            return false;
        }

        QString mean_file = paras.empty() ? "" : paras[k]; if(mean_file == "NULL") mean_file = ""; k++;
        if(mean_file.isEmpty())
        {
            cerr<<"Need a mean_file"<<endl;
            return false;
        }

        QString  outswc_file =  SWCfileName + "_pruned.swc";
        if(!outfiles.empty())   outswc_file =  outfiles[0];

        cout<<"inimg_file = "<<inimg_file.toStdString().c_str()<<endl;
        cout<<"inswc_file = "<<SWCfileName.toStdString().c_str()<<endl;
        cout<<"outswc_file = "<<outswc_file.toStdString().c_str()<<endl;
        cout<<"model_file = "<<model_file.toStdString().c_str()<<endl;
        cout<<"trained_file = "<<trained_file.toStdString().c_str()<<endl;
        cout<<"mean_file = "<<mean_file.toStdString().c_str()<<endl;

        unsigned char * data1d = 0;
        V3DLONG in_sz[4];

        int datatype;
        if(!simple_loadimage_wrapper(callback, inimg_file.toStdString().c_str(), data1d, in_sz, datatype))
        {
            cerr<<"load image "<<inimg_file.toStdString().c_str()<<" error!"<<endl;
            return false;
        }

        V3DLONG N = in_sz[0];
        V3DLONG M = in_sz[1];
        V3DLONG P = in_sz[2];
        V3DLONG sc = in_sz[3];

        NeuronTree nt = readSWC_file(SWCfileName);
        unsigned int Wx=30, Wy=30, Wz=15;
        Classifier classifier(model_file.toStdString(), trained_file.toStdString(), mean_file.toStdString());
        std::vector<cv::Mat> imgs;
        V3DLONG num_patches = 0;
        std::vector<std::vector<float> > outputs_overall;
        std::vector<std::vector<float> > outputs;
        for (V3DLONG i=0;i<nt.listNeuron.size();i++)
        {
            V3DLONG tmpx = nt.listNeuron.at(i).x;
            V3DLONG tmpy = nt.listNeuron.at(i).y;
            V3DLONG tmpz = nt.listNeuron.at(i).z;

            V3DLONG xb = tmpx-1-Wx; if(xb<0) xb = 0;if(xb>=N-1) xb = N-1;
            V3DLONG xe = tmpx-1+Wx; if(xe>=N-1) xe = N-1;
            V3DLONG yb = tmpy-1-Wy; if(yb<0) yb = 0;if(yb>=M-1) yb = M-1;
            V3DLONG ye = tmpy-1+Wy; if(ye>=M-1) ye = M-1;
            V3DLONG zb = tmpz-1-Wz; if(zb<0) zb = 0;if(zb>=P-1) zb = P-1;
            V3DLONG ze = tmpz-1+Wz; if(ze>=P-1) ze = P-1;

            V3DLONG im_cropped_sz[4];
            im_cropped_sz[0] = xe - xb + 1;
            im_cropped_sz[1] = ye - yb + 1;
            im_cropped_sz[2] = 1;
            im_cropped_sz[3] = sc;

            unsigned char *im_cropped = 0;

            V3DLONG pagesz = im_cropped_sz[0]* im_cropped_sz[1]* im_cropped_sz[2]*im_cropped_sz[3];
            try {im_cropped = new unsigned char [pagesz];}
            catch(...)  {v3d_msg("cannot allocate memory for im_cropped."); return false;}
            memset(im_cropped, 0, sizeof(unsigned char)*pagesz);

            for(V3DLONG iz = zb; iz <= ze; iz++)
            {
                V3DLONG offsetk = iz*M*N;
                V3DLONG j = 0;
                for(V3DLONG iy = yb; iy <= ye; iy++)
                {
                    V3DLONG offsetj = iy*N;
                    for(V3DLONG ix = xb; ix <= xe; ix++)
                    {
                        if(data1d[offsetk + offsetj + ix] >= im_cropped[j])
                            im_cropped[j] = data1d[offsetk + offsetj + ix];
                        j++;
                    }
                }
            }

            cv::Mat img(im_cropped_sz[1], im_cropped_sz[0], CV_8UC1, im_cropped);
            imgs.push_back(img);

            if(num_patches >=10000)
            {
                outputs = classifier.Predict(imgs);
                for(V3DLONG d = 0; d<outputs.size();d++)
                    outputs_overall.push_back(outputs[d]);
                outputs.clear();
                imgs.clear();
                num_patches = 0;
            }else
                num_patches++;
        }

        if(imgs.size()>0)
        {
            outputs = classifier.Predict(imgs);
            for(V3DLONG d = 0; d<outputs.size();d++)
                outputs_overall.push_back(outputs[d]);
        }

 //       std::vector<std::vector<float> > outputs = classifier.Predict(imgs);
        double p_num = 0;
        double n_num = 0;
        QList <ImageMarker> marklist;
        for (V3DLONG j=0;j<nt.listNeuron.size();j++)
        {
            std::vector<float> output = outputs_overall[j];
            if(output.at(0) > output.at(1))
            {
                ImageMarker S;
                S.x = nt.listNeuron.at(j).x;
                S.y = nt.listNeuron.at(j).y;
                S.z = nt.listNeuron.at(j).z;
                marklist.append(S);
                n_num++;
            }
            else
                p_num++;
        }

        NeuronTree nt_DL = DL_eliminate_swc(nt,marklist);
        NeuronTree nt_DL_sort = SortSWC_pipeline(nt_DL.listNeuron,VOID, 10);
        writeSWC_file(outswc_file, nt_DL_sort);

        QString outswc_file_pruned = outswc_file + "_final.swc";

        NeuronTree nt_DL_sort_pruned = remove_swc(nt_DL_sort,50);

        writeSWC_file(outswc_file_pruned, nt_DL_sort_pruned);
        outputs_overall.clear();
        imgs.clear();
        if(data1d) {delete []data1d; data1d = 0;}
        return true;
    }
    else if(func_name == tr("3D_Axon_detection"))
    {
        cout<<"Welcome to Caffe 3D axon detection plugin"<<endl;
        if(infiles.empty())
        {
            cerr<<"Need input image file"<<endl;
            return false;
        }
        QString  inimg_file =  infiles[0];
        int k=0;

        QString model_file = paras.empty() ? "" : paras[k]; if(model_file == "NULL") model_file = ""; k++;
        if(model_file.isEmpty())
        {
            cerr<<"Need a model_file"<<endl;
            return false;
        }

        QString trained_file = paras.empty() ? "" : paras[k]; if(trained_file == "NULL") trained_file = ""; k++;
        if(trained_file.isEmpty())
        {
            cerr<<"Need a trained_file"<<endl;
            return false;
        }

        QString mean_file = paras.empty() ? "" : paras[k]; if(mean_file == "NULL") mean_file = ""; k++;
        if(mean_file.isEmpty())
        {
            cerr<<"Need a mean_file"<<endl;
            return false;
        }

        int Sxy = paras.empty() ? 10 : atoi(paras[k]);


        cout<<"inimg_file = "<<inimg_file.toStdString().c_str()<<endl;
        cout<<"model_file = "<<model_file.toStdString().c_str()<<endl;
        cout<<"trained_file = "<<trained_file.toStdString().c_str()<<endl;
        cout<<"mean_file = "<<mean_file.toStdString().c_str()<<endl;
        cout<<"sample_size = "<<Sxy<<endl;

        unsigned char * data1d = 0;
        V3DLONG in_sz[4];

        int datatype;
        if(!simple_loadimage_wrapper(callback, inimg_file.toStdString().c_str(), data1d, in_sz, datatype))
        {
            cerr<<"load image "<<inimg_file.toStdString().c_str()<<" error!"<<endl;
            return false;
        }

        V3DLONG N = in_sz[0];
        V3DLONG M = in_sz[1];
        V3DLONG P = in_sz[2];

        V3DLONG pagesz_mip = in_sz[0]*in_sz[1];
        unsigned char *data1d_mip=0;
        try {data1d_mip = new unsigned char [pagesz_mip];}
        catch(...)  {v3d_msg("cannot allocate memory for image_mip."); return false;}
        for(V3DLONG iy = 0; iy < M; iy++)
        {
            V3DLONG offsetj = iy*N;
            for(V3DLONG ix = 0; ix < N; ix++)
            {
                int max_mip = 0;
                for(V3DLONG iz = 0; iz < P; iz++)
                {
                    V3DLONG offsetk = iz*M*N;
                    if(data1d[offsetk + offsetj + ix] >= max_mip)
                    {
                        data1d_mip[iy*N + ix] = data1d[offsetk + offsetj + ix];
                        max_mip = data1d[offsetk + offsetj + ix];
                    }
                }
            }
        }

        Classifier classifier(model_file.toStdString(), trained_file.toStdString(), mean_file.toStdString());
        std::vector<std::vector<float> > detection_results = batch_detection(data1d_mip,classifier,N,M,1,Sxy);

        mean_shift_fun fun_obj;
        LandmarkList marklist_2D;
        QList <ImageMarker> marklist_2D_saved;
        ImageMarker S_2D;

        QString markerpath2D =  inimg_file + QString("_2D.marker");
        V3DLONG d = 0;
        for(V3DLONG iy = Sxy; iy < M; iy = iy+Sxy)
        {
            for(V3DLONG ix = Sxy; ix < N; ix = ix+Sxy)
            {
                std::vector<float> output = detection_results[d];
                if(output.at(1) > output.at(0))
                {
                    LocationSimple S;
                    S.x = ix;
                    S.y = iy;
                    S.z = 1;
                    marklist_2D.push_back(S);

                    S_2D.x = ix;
                    S_2D.y = iy;
                    S_2D.z = 1;
                    S_2D.color.r = 255;
                    S_2D.color.g = 0;
                    S_2D.color.b = 0;
                    marklist_2D_saved.append(S_2D);
                }
                d++;
            }
        }
        detection_results.clear();

        writeMarker_file(markerpath2D.toStdString().c_str(),marklist_2D_saved);


        //mean shift
        LandmarkList marklist_2D_shifted;
        QList <ImageMarker> marklist_2D_shifted_saved;

        vector<V3DLONG> poss_landmark;
        vector<float> mass_center;
        double windowradius = Sxy+5;

        V3DLONG sz_img[4];
        sz_img[0] = N; sz_img[1] = M; sz_img[2] = 1; sz_img[3] = 1;
        fun_obj.pushNewData<unsigned char>((unsigned char*)data1d_mip, sz_img);
        poss_landmark=landMarkList2poss(marklist_2D, sz_img[0], sz_img[0]*sz_img[1]);

        for (V3DLONG j=0;j<poss_landmark.size();j++)
        {
            mass_center=fun_obj.mean_shift_center(poss_landmark[j],windowradius);
            LocationSimple tmp(mass_center[0]+1,mass_center[1]+1,mass_center[2]+1);
            marklist_2D_shifted.append(tmp);

            S_2D.x = tmp.x;
            S_2D.y = tmp.y;
            S_2D.z = tmp.z;
            S_2D.color.r = 255;
            S_2D.color.g = 0;
            S_2D.color.b = 0;
            marklist_2D_shifted_saved.append(S_2D);
        }

        QString markerpath2Dshifted =  inimg_file + QString("_2D_shifted.marker");
        writeMarker_file(markerpath2Dshifted.toStdString().c_str(),marklist_2D_shifted_saved);


        QList <ImageMarker> marklist_3D;
        QString markerpath =  inimg_file + QString("_3D_final.marker");
        ImageMarker S;
        NeuronTree nt;
        QList <NeuronSWC> & listNeuron = nt.listNeuron;


        for(V3DLONG i = 0; i < marklist_2D_shifted.size(); i++)
        {
            V3DLONG ix = marklist_2D_shifted.at(i).x;
            V3DLONG iy = marklist_2D_shifted.at(i).y;
            double I_max = 0;
            V3DLONG iz;
            for(V3DLONG j = 0; j < P; j++)
            {
                if(data1d[j*M*N + iy*N + ix] >= I_max)
                {
                    I_max = data1d[j*M*N + iy*N + ix];
                    iz = j;
                }

            }
            S.x = ix;
            S.y = iy;
            S.z = iz;
            S.color.r = 255;
            S.color.g = 0;
            S.color.b = 0;
            marklist_3D.append(S);

            NeuronSWC n;
            n.x = ix-1;
            n.y = iy-1;
            n.z = iz-1;
            n.n = i;
            n.type = 2;
            n.r = 1;
            n.pn = -1; //so the first one will be root
            listNeuron << n;
        }
        QList <ImageMarker> marklist_3D_pruned = batch_deletion(data1d,classifier,marklist_3D,N,M,P);
        writeMarker_file(markerpath.toStdString().c_str(),marklist_3D_pruned);

//        NeuronTree nt_sorted = SortSWC_pipeline(nt.listNeuron,VOID, 40);
//        QList<NeuronSWC> newNeuron_connected;
//        double angthr=cos((60-60)/180*M_PI);
//        QString  swc_processed = inimg_file + "_axon_3D.swc";

//        connectall(&nt_sorted, newNeuron_connected, 1, 1, 1, angthr, 100, 1, false, -1);

//        if(!export_list2file(newNeuron_connected, swc_processed,swc_processed)){
//            qDebug()<<"error: Cannot open file "<<swc_processed<<" for writing!"<<endl;
//        }
    }
    else if (func_name == tr("help"))
	{
        cout<<"Usage : v3d -x prediction_caffe -f Prediction -i <inimg_folder> -p <model_file> <trained_file> <mean_file>"<<endl;
        cout<<endl;
        cout<<"Usage : v3d -x prediction_caffe -f Quality_Assess -i <inimg_file> -p <inswc_file> <model_file> <trained_file> <mean_file>"<<endl;
        cout<<endl;
        cout<<"Usage : v3d -x prediction_caffe -f Detection -i <inimg_file> -p <model_file> <trained_file> <mean_file> <sample_size>"<<endl;
        cout<<endl;
        return true;
	}
	else return false;

	return true;
}

