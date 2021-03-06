/* IVSCC_process_swc_plugin.cpp
 * This is a test plugin, you can use it as a demo.
 * 2014-10-23 : by Zhi Zhou
 */
 
#include "v3d_message.h"
#include <vector>
#include "IVSCC_process_swc_plugin.h"
#include "basic_surf_objs.h"
#include <boost/lexical_cast.hpp>

#include <iostream>
#include "../../../released_plugins/v3d_plugins/neurontracing_vn2/app2/my_surf_objs.h"
#include "../AllenNeuron_postprocessing/sort_swc_IVSCC.h"
#include "../APP2_large_scale/readRawfile_func.h"



using namespace std;
#define getParent(n,nt) ((nt).listNeuron.at(n).pn<0)?(1000000000):((nt).hashNeuron.value((nt).listNeuron.at(n).pn))
#define dist2D(a,b) sqrt(((a).x-(b).x)*((a).x-(b).x)+((a).y-(b).y)*((a).y-(b).y))
#define DOTP(a,b,c) (((b).x-(a).x)*((c).x-(b).x)+((b).y-(a).y)*((c).y-(b).y))


//template <class T> T pow2(T a)
//{
//    return a*a;

//}

Q_EXPORT_PLUGIN2(IVSCC_process_swc, IVSCC_process_swc);
 
bool export_list2file_IVSCC(QList<NeuronSWC> & lN, QString fileSaveName, QString fileOpenName)
{
    QFile file(fileSaveName);
    if (!file.open(QIODevice::WriteOnly|QIODevice::Text))
        return false;
    QTextStream myfile(&file);

    QFile qf(fileOpenName);
    if (! qf.open(QIODevice::ReadOnly | QIODevice::Text))
    {
#ifndef DISABLE_V3D_MSG
        v3d_msg(QString("open file [%1] failed!").arg(fileOpenName));
#endif
        return false;
    }
    QString info;
    while (! qf.atEnd())
    {
        char _buf[1000], *buf;
        qf.readLine(_buf, sizeof(_buf));
        for (buf=_buf; (*buf && *buf==' '); buf++); //skip space

        if (buf[0]=='\0')	continue;
        if (buf[0]=='#')
        {
           info = buf;
           myfile<< info.remove('\n') <<endl;
        }

    }

    for (V3DLONG i=0;i<lN.size();i++)
        myfile << lN.at(i).n <<" " << lN.at(i).type << " "<< lN.at(i).x <<" "<<lN.at(i).y << " "<< lN.at(i).z << " "<< lN.at(i).r << " " <<lN.at(i).pn << "\n";

    file.close();
    cout<<"swc file "<<fileSaveName.toStdString()<<" has been generated, size: "<<lN.size()<<endl;
    return true;
};

QStringList IVSCC_process_swc::menulist() const
{
	return QStringList() 
		<<tr("about");
}

QStringList IVSCC_process_swc::funclist() const
{
	return QStringList()
        <<tr("process")
        <<tr("process_v2")
        <<tr("process_remove_artifacts")
		<<tr("help");
}

void IVSCC_process_swc::domenu(const QString &menu_name, V3DPluginCallback2 &callback, QWidget *parent)
{
    if (menu_name == tr("about"))
	{
        v3d_msg(tr("This is a plugin only worked using command line to post process swc files in IVSCC pipeline. "
            "Developed by Zhi Zhou, 2014-10-23"));
	}
}

bool IVSCC_process_swc::dofunc(const QString & func_name, const V3DPluginArgList & input, V3DPluginArgList & output, V3DPluginCallback2 & callback,  QWidget * parent)
{
    vector<char*> infiles, paras, outfiles;
	if(input.size() >= 1) infiles = *((vector<char*> *)input.at(0).p);
    if(input.size() >= 2) paras = *((vector<char*> *)input.at(1).p);
	if(output.size() >= 1) outfiles = *((vector<char*> *)output.at(0).p);

    if (func_name == tr("process"))
	{
        cout<<"Welcome to IVSCC swc post processing plugin"<<endl;
        if(infiles.empty())
        {
            cerr<<"Need input swc file"<<endl;
            return false;
        }

        QString  inswc_file =  infiles[0];
        int k=0;

        QString inmarker_file = paras.empty() ? "" : paras[k]; if(inmarker_file == "NULL") inmarker_file = "";
        if(inmarker_file.isEmpty())
        {
            cerr<<"Need a marker file"<<endl;
            return false;
        }

        QString  outswc_file =  outfiles[0];
        cout<<"inswc_file = "<<inswc_file.toStdString().c_str()<<endl;
        cout<<"inmarker_file = "<<inmarker_file.toStdString().c_str()<<endl;
        cout<<"outswc_file = "<<outswc_file.toStdString().c_str()<<endl;

        vector<MyMarker> file_inmarkers;
        file_inmarkers = readMarker_file(string(qPrintable(inmarker_file)));

        double length = 15;
        double sum_x = 0, sum_y = 0;
        for(int i = 0; i < file_inmarkers.size(); i ++)
        {
            sum_x+= file_inmarkers[i].x;
            sum_y+= file_inmarkers[i].y;
        }

        double cent_x = sum_x/file_inmarkers.size();
        double cent_y = sum_y/file_inmarkers.size();

        double sum_r = 0;
        for(int i = 0; i < file_inmarkers.size(); i ++)
        {
            sum_r+= sqrt(pow2(file_inmarkers[i].x - cent_x) + pow2(file_inmarkers[i].y - cent_y));
        }

        double soma_r = sum_r/file_inmarkers.size();

        NeuronTree nt = readSWC_file(inswc_file);

        QVector<QVector<V3DLONG> > childs;


        V3DLONG neuronNum = nt.listNeuron.size();
        childs = QVector< QVector<V3DLONG> >(neuronNum, QVector<V3DLONG>() );
        V3DLONG *flag = new V3DLONG[neuronNum];

        for (V3DLONG i=0;i<neuronNum;i++)
        {
            flag[i] = 1;

            V3DLONG par = nt.listNeuron[i].pn;
            if (par<0) continue;
            childs[nt.hashNeuron.value(par)].push_back(i);
        }

        QList<NeuronSWC> list = nt.listNeuron;
        for (int i=0;i<list.size();i++)
        {
            if (childs[i].size()==0)
            {
                int index_tip = 0;
                int parent_tip = getParent(i,nt);
                while(childs[parent_tip].size()<2)
                {

                    parent_tip = getParent(parent_tip,nt);
                    index_tip++;
                }
                if(index_tip < length)
                {
                    flag[i] = -1;

                    int parent_tip = getParent(i,nt);
                    while(childs[parent_tip].size()<2)
                    {
                        flag[parent_tip] = -1;
                        parent_tip = getParent(parent_tip,nt);
                    }
                }

            }

        }

       //NeutronTree structure
       NeuronTree nt_prunned;
       QList <NeuronSWC> listNeuron;
       QHash <int, int>  hashNeuron;
       listNeuron.clear();
       hashNeuron.clear();

       //set node
       NeuronSWC S;
       for (int i=0;i<list.size();i++)
       {
           if(flag[i] == 1)
           {
                NeuronSWC curr = list.at(i);
                S.n 	= curr.n;
                if(i ==0)
                {
                     S.type = 1;
                     S.r = soma_r;
                }
                else
                {
                    S.type 	= 3;
                    S.r 	= curr.r;
                }
                S.x 	= curr.x;
                S.y 	= curr.y;
                S.z 	= curr.z;
                S.pn 	= curr.pn;
                listNeuron.append(S);
                hashNeuron.insert(S.n, listNeuron.size()-1);
           }

      }
       nt_prunned.n = -1;
       nt_prunned.on = true;
       nt_prunned.listNeuron = listNeuron;
       nt_prunned.hashNeuron = hashNeuron;

       if(flag) {delete[] flag; flag = 0;}

       export_list2file_IVSCC(nt_prunned.listNeuron,outswc_file,inswc_file);

       nt = nt_prunned;
       neuronNum = nt.listNeuron.size();
       childs = QVector< QVector<V3DLONG> >(neuronNum, QVector<V3DLONG>() );
       for (V3DLONG i=0;i<neuronNum;i++)
       {
           V3DLONG par = nt.listNeuron[i].pn;
           if (par<0) continue;
           childs[nt.hashNeuron.value(par)].push_back(i);
       }

       vector<MyMarker*> final_out_swc = readSWC_file(outswc_file.toStdString());
       vector<MyMarker*> final_out_swc_updated;
       final_out_swc_updated.push_back(final_out_swc[0]);


       for(int j = 1; j < final_out_swc.size(); j++)
       {
           int flag_prun = 0;
           int par_x = final_out_swc[j]->parent->x;
           int par_y = final_out_swc[j]->parent->y;
           int par_z = final_out_swc[j]->parent->z;
           int par_r = final_out_swc[j]->parent->radius;

           int dis_prun = sqrt(pow2(final_out_swc[j]->x - par_x) + pow2(final_out_swc[j]->y - par_y) + pow2(final_out_swc[j]->z - par_z));
           if( (final_out_swc[j]->radius + par_r - dis_prun)/dis_prun > 0.3)
           {
               if(childs[j].size() > 0)
               {
                   for(int jj = 0; jj < childs[j].size(); jj++)
                   final_out_swc[childs[j].at(jj)]->parent = final_out_swc[j]->parent;
               }
               flag_prun = 1;
           }

           if(flag_prun == 0)
           {
              final_out_swc_updated.push_back(final_out_swc[j]);
           }
       }

       saveSWC_file(outswc_file.toStdString(), final_out_swc_updated);

    }
    else if (func_name == tr("process_v2"))
    {
        cout<<"Welcome to IVSCC swc post processing plugin v2"<<endl;
        if(infiles.empty())
        {
            cerr<<"Need input swc file"<<endl;
            return false;
        }

        QString  inswc_file =  infiles[0];
        QString  inmarkerpath_file = infiles[1];
        if(inmarkerpath_file.isEmpty())
        {
            cerr<<"Need a marker path file"<<endl;
            return false;
        }

        QString inmarker_file = infiles[2];
        if(inmarker_file.isEmpty())
        {
            cerr<<"Need a marker file"<<endl;
            return false;
        }

        QString in2Draw_file = infiles[3];
        if(in2Draw_file.isEmpty())
        {
            cerr<<"Need a 2D raw file"<<endl;
            return false;
        }

        QString  outswc_file =  outfiles[0];
        cout<<"inswc_file = "<<inswc_file.toStdString().c_str()<<endl;
        cout<<"inmarkerpath_file = "<<inmarkerpath_file.toStdString().c_str()<<endl;
        cout<<"inmarker_file = "<<inmarker_file.toStdString().c_str()<<endl;
        cout<<"in2Draw_file = "<<in2Draw_file.toStdString().c_str()<<endl;
        cout<<"outswc_file = "<<outswc_file.toStdString().c_str()<<endl;

        vector<MyMarker> file_inmarkers;
        file_inmarkers = readMarker_file(string(qPrintable(inmarkerpath_file)));

        double sum_x = 0, sum_y = 0;
        for(int i = 0; i < file_inmarkers.size(); i ++)
        {
            sum_x+= file_inmarkers[i].x;
            sum_y+= file_inmarkers[i].y;
        }

        double cent_x = sum_x/file_inmarkers.size();
        double cent_y = sum_y/file_inmarkers.size();

        double sum_r = 0;
        for(int i = 0; i < file_inmarkers.size(); i ++)
        {
            sum_r+= sqrt(pow2(file_inmarkers[i].x - cent_x) + pow2(file_inmarkers[i].y - cent_y));
        }

        double soma_r = sum_r/file_inmarkers.size();


        vector<MyMarker> center_inmarkers;
        center_inmarkers = readMarker_file(string(qPrintable(inmarker_file)));

        unsigned char * data1d = 0;
        V3DLONG in_sz[4];
        int datatype;

        if (!simple_loadimage_wrapper(callback,in2Draw_file.toStdString().c_str(), data1d, in_sz, datatype))
        {
            fprintf (stderr, "Error happens in reading the subject file [%0]. Exit. \n",in2Draw_file.toStdString().c_str());
            return false;
        }

        double soma_x = center_inmarkers[0].x;
        double soma_y = center_inmarkers[0].y;
        double soma_z = center_inmarkers[0].z;

        if(data1d) {delete []data1d; data1d = 0;}

        NeuronTree nt = readSWC_file(inswc_file);
        double Dist = 10000000000;
        V3DLONG soma_ID = -1;
        for (V3DLONG i = 0; i < nt.listNeuron.size(); i++)
        {
            double point_x = nt.listNeuron.at(i).x;
            double point_y = nt.listNeuron.at(i).y;
            double point_z = nt.listNeuron.at(i).z;
            if(sqrt(pow2(point_x - soma_x) + pow2(point_y - soma_y) + pow2(point_z - soma_z)) < Dist)
            {
                Dist = sqrt(pow2(point_x - soma_x) + pow2(point_y - soma_y) + pow2(point_z - soma_z));
                soma_ID = i;
            }
        }

        V3DPluginArgItem arg;
        V3DPluginArgList input_sort;
        V3DPluginArgList output;

        arg.type = "random";std::vector<char*> arg_input_sort;
        char* fileName_string = infiles[0];
        arg_input_sort.push_back(fileName_string);
        arg.p = (void *) & arg_input_sort; input_sort<< arg;
        arg.type = "random";std::vector<char*> arg_sort_para; arg_sort_para.push_back("0");
        string S_soma_ID = boost::lexical_cast<string>(soma_ID+1);
        char* C_soma_ID = new char[S_soma_ID.length() + 1];
        strcpy(C_soma_ID,S_soma_ID.c_str());
        arg_sort_para.push_back(C_soma_ID);
        arg.p = (void *) & arg_sort_para; input_sort << arg;
        QString full_plugin_name_sort = "sort_neuron_swc";
        QString func_name_sort = "sort_swc";
        callback.callPluginFunc(full_plugin_name_sort,func_name_sort, input_sort,output);

        QString swcfilename_sorted = inswc_file + "_sorted.swc";
        vector<MyMarker*> temp_out_swc = readSWC_file(swcfilename_sorted.toStdString());
        for(V3DLONG i = 0; i < temp_out_swc.size(); i++)
        {
            if(i ==0)
            {
                temp_out_swc[i]->radius = soma_r;
                temp_out_swc[i]->type = 1;
            }
            else
                temp_out_swc[i]->type = 3;
        }
        saveSWC_file(outswc_file.toStdString(), temp_out_swc);
    }
    else if (func_name == tr("process_remove_artifacts"))
    {
        cout<<"Welcome to IVSCC 2D swc post processing plugin"<<endl;
        if(infiles.empty())
        {
            cerr<<"Need input swc file"<<endl;
            return false;
        }
        QString  inswc_file =  infiles[0];


        QString  inimage_file = infiles[1];
        if(inimage_file.isEmpty())
        {
            cerr<<"Need an 3D image path file"<<endl;
            return false;
        }

        QString  outswc_file =  outfiles[0];
        cout<<"inswc_file = "<<inswc_file.toStdString().c_str()<<endl;
        cout<<"inimage_file = "<<inimage_file.toStdString().c_str()<<endl;
        cout<<"outswc_file = "<<outswc_file.toStdString().c_str()<<endl;


        unsigned char * datald = 0;
        V3DLONG *in_zz = 0;
        V3DLONG *in_sz = 0;

        int datatype;

        if (!loadRawRegion(const_cast<char *>(inimage_file.toStdString().c_str()), datald, in_zz, in_sz,datatype,0,0,0,1,1,1))
        {
            return false;
        }

        if(datald) {delete []datald; datald = 0;}
        if(in_sz) {delete []in_sz; in_sz = 0;}

        NeuronTree nt = readSWC_file(inswc_file);
        nt = SortSWC_pipeline(nt.listNeuron,1000000000, 0);

        V3DLONG end_ID = 0,start_ID = 0;
        for (V3DLONG i = 1; i<nt.listNeuron.size(); i++)
        {
            if(nt.listNeuron.at(i).parent <= 0)
            {
                NeuronTree sub_nt = nt;
                sub_nt.listNeuron.erase(sub_nt.listNeuron.begin()+i,sub_nt.listNeuron.end());
                if(end_ID > 0)
                {
                    sub_nt.listNeuron.erase(sub_nt.listNeuron.begin(),sub_nt.listNeuron.begin()+ end_ID);
                    start_ID = end_ID;
                }
                end_ID = i;

                NeuronTree sub_nt_sort = SortSWC_pipeline(sub_nt.listNeuron,1000000000, 0);
                double max_distance = 0;
                double total_length = 0;
                double max_x = 0;
                double max_y = 0;
                double z_step = 0;
 //               bool flag_90 = false;
                V3DLONG d = 0;
                for(V3DLONG ii = 0; ii <sub_nt_sort.listNeuron.size();ii++)
                {
                    double x_ii = sub_nt_sort.listNeuron[ii].x;
                    double y_ii = sub_nt_sort.listNeuron[ii].y;
                    z_step += sub_nt_sort.listNeuron[ii].z;
                    d++;
                    int parent = getParent(ii,sub_nt_sort);
                    if (parent==1000000000) continue;
                    total_length += dist2D(sub_nt_sort.listNeuron.at(ii),sub_nt_sort.listNeuron.at(parent));
                    for(V3DLONG jj = ii+1; jj <sub_nt.listNeuron.size();jj++)
                    {
                        double x_jj = sub_nt_sort.listNeuron[jj].x;
                        double y_jj = sub_nt_sort.listNeuron[jj].y;
                        if(sqrt(pow2(x_ii - x_jj) + pow2(y_ii - y_jj)) > max_distance)
                            max_distance = sqrt(pow2(x_ii - x_jj) + pow2(y_ii - y_jj));
                        if(fabs(x_ii - x_jj) > max_x) max_x = fabs(x_ii - x_jj);
                        if(fabs(y_ii - y_jj) > max_y) max_y = fabs(y_ii - y_jj);
                    }
//                    if(!flag_90)
//                    {
//                        int parent_2nd = getParent(parent,sub_nt_sort);
//                        if (parent_2nd==1000000000) continue;

//                        double length1 = dist2D(sub_nt_sort.listNeuron[ii],sub_nt_sort.listNeuron[parent]);
//                        double length2 = dist2D(sub_nt_sort.listNeuron[parent],sub_nt_sort.listNeuron[parent_2nd]);
//                        double cosAng=DOTP(sub_nt_sort.listNeuron[ii],sub_nt_sort.listNeuron[parent],sub_nt_sort.listNeuron[parent_2nd])/(length1*length2);

//                        if(fabs(cosAng) < 0.15 && before_angle > 0.95)
//                        {
//                            v3d_msg(QString("angle is %1, before is %2,id is %3,").arg(cosAng).arg(start_ID+ii).arg(before_angle),0);
//                            nt.listNeuron[start_ID+ii].radius = 30;
//                            flag_90 = true;
//                           // break;
//                        }

//                        before_angle = cosAng;
//                    }


                }

                double ratio_check = (total_length-max_distance)/max_distance;
                double avarge_zstep = z_step/d;
                if(avarge_zstep > 0.9* in_zz[2]||(ratio_check < 0.2 && (max_x/max_y < 0.05 || max_y/max_x < 0.05)) || (total_length <300 && max_x < 100 && max_y < 100))
                {
             //       v3d_msg(QString("ratio is %1,max_x is %2, max_y is %3").arg(ratio_check).arg(max_x).arg(max_y));

                    for(V3DLONG d = start_ID; d < i; d++)
                        nt.listNeuron[d].type = 0;
                }


            }
        }

        QList<NeuronSWC> list = nt.listNeuron;
        NeuronTree nt_prunned;
        QList <NeuronSWC> listNeuron;
        QHash <int, int>  hashNeuron;
        listNeuron.clear();
        hashNeuron.clear();

        //set node

        NeuronSWC S;
        for (int i=0;i<list.size();i++)
        {
            if(list.at(i).type != 0)
            {
                 NeuronSWC curr = list.at(i);
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

        writeSWC_file(outswc_file,nt_prunned);

    }
    else if (func_name == tr("help"))
    {
        cout<<"Usage : v3d -x dllname -f process -i <inswc_folder> -o <outswc_file> -p <somapath_marker>"<<endl;
        cout<<endl;
    }
    else return false;;

	return true;
}

