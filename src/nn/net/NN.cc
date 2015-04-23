// Copyright (c) 2015 Qihoo 360 Technology Co. Ltd
// Author: Yu Shengkai (yushengkai@360.cn)

#include <cblas.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/random.hpp>
#include <glog/logging.h>

#include "main/util.h"
#include "main/NN.h"

static boost::mt19937 rng(static_cast<unsigned>(std::time(0)));
boost::normal_distribution<double> norm_dist1(0, 0.1);
boost::variate_generator<boost::mt19937&, boost::normal_distribution<double> >
normal_sample1(rng, norm_dist1);

bool NN::Init(LookupTable * lookup_table, std::string param,
              int m, std::string init_type, bool wb) {
  learning_rate = 0.005;
  with_bias=wb;
  minibatchsize = m;
  softmax_sum = new double[minibatchsize];
  inputsize = lookup_table->GetOutputWidth();
  layersizes.push_back(inputsize);
  boost::trim(param);
  std::vector<std::string> parts;
  boost::split(parts, param, boost::is_any_of(":"));
  std::cout<<"nn layers:"<<inputsize;
  for(unsigned int i=0;i<parts.size();i++) {
    int value = boost::lexical_cast<int>(parts[i]);
    if(value<=0) {
      return false;
    }
    std::cout<<"->"<<value;
    layersizes.push_back(value);
  }
  outputsize = layersizes.back();
  std::cout<<std::endl;
  for(unsigned int i=0;i<layersizes.size();i++) {
    if(i==0) {
      double* layer_value = NULL;
      layer_values.push_back(layer_value);
      continue;
    }

    int layer_value_size = minibatchsize * layersizes[i];
    double* layer_value = new double[layer_value_size];
    layer_values.push_back(layer_value);

    int node_num = layersizes[i];
    int weight_num_of_node = layersizes[i-1];
    int weight_num = weight_num_of_node * node_num;
    double* weight_matrix = new double[weight_num];
    double* bias_vector = new double[node_num];
    weight_matrixs.push_back(weight_matrix);
    bias_vectors.push_back(bias_vector);

    double* delta = new double[weight_num];
    double* error = new double[layer_value_size];
    delta_matrixs.push_back(delta);
    error_matrixs.push_back(error);
  }

  nn_output = layer_values.back();
  InitWeight(init_type);
  return true;
}

void NN::InitWeight(std::string init_type) {
  for(unsigned int i=1;i<layersizes.size();i++) {
    int node_num = layersizes[i];
    int weight_num_of_node = layersizes[i-1];
    int weight_num = node_num * weight_num_of_node;
    double* weight_matrix = weight_matrixs[i-1];
    double* bias_vector = bias_vectors[i-1];
    for(int j=0;j<weight_num;j++) {
      if(init_type=="normal")
      {
        weight_matrix[j] = normal_sample1();
      } else if(init_type == "123") {
        weight_matrix[j] = (double)(j+1);
      } else if(init_type == "1") {
        weight_matrix[j] = 1;
      } else if (init_type == "0") {
        weight_matrix[j] = 0;
      } else {
        weight_matrix[j] = normal_sample1();
      }
    }
    for(int j=0;j<node_num;j++) {
      if(init_type=="normal")
      {
        bias_vector[j] = normal_sample1();
      } else if(init_type == "123") {
        bias_vector[j] = (double)(j+1);
      } else if(init_type == "1") {
        bias_vector[j] = 1;
      } else if (init_type == "0") {
        bias_vector[j] = 0;
      } else {
        bias_vector[j] = normal_sample1();
      }
    }
  }
  if(!with_bias) {
    for(unsigned int i=1;i<layersizes.size();i++) {
      int node_num = layersizes[i];
      double* bias_vector = bias_vectors[i-1];
      for(int j=0;j<node_num;j++) {
        bias_vector[j] = 0;
      }
    }
  }
}


bool NN::Forward(double* input, int batchsize) {
  layer_values[0] = input;
  /*
   * minibatch
   * minibatchsize * inputsize  = real input size
   *  */
  for(int layer=1;layer<layersizes.size();layer++) {

    enum CBLAS_ORDER Order=CblasRowMajor;
    enum CBLAS_TRANSPOSE TransX=CblasNoTrans;
    enum CBLAS_TRANSPOSE TransW=CblasTrans;
    int M=batchsize;//X的行数，O的行数
    int N=layersizes[layer];//W的列数，O的列数
    int K=layersizes[layer-1];//X的列数，W的行数
    double alpha=1;
    double beta=0;
    int lda=K;
    int ldb=K;
    int ldc=N;
    double* X = layer_values[layer-1];
    double* W= weight_matrixs[layer-1];
    double* output = layer_values[layer];
    cblas_dgemm(Order, TransX, TransW, M, N, K,
                alpha, X, lda,
                W, ldb,
                beta, output, ldc);
    for(int i=0;i<batchsize;i++) {
      for(int j=0;j<layersizes[layer];j++) {
        int idx = i * layersizes[layer] + j;
        if(layer == layersizes.size()-1) {
          output[idx] = exp(output[idx]);
        }else {
          output[idx] = sigmoid(output[idx]);
        }
      }
    }

    if(layer == layersizes.size()-1){
      double* ones = new double[outputsize];
      for(int i=0;i<outputsize;i++) {
        ones[i] = 1;
      }
      Order = CblasRowMajor;
      TransX = CblasNoTrans;
      TransW = CblasTrans;
      M=batchsize;
      N=1;
      K=layersizes[layer];
      alpha=1;
      beta = 0;
      lda=K;
      ldb=K;
      ldc=N;
      cblas_dgemm(Order, TransX, TransW,
                  M, N, K,
                  alpha, output, lda,
                  ones, ldb,
                  beta, softmax_sum, ldc
                 );

      for(int i=0;i<batchsize;i++) {
        for(int j=0;j<layersizes[layer];j++) {
          int idx = i*layersizes[layer]+j;
          output[idx] = output[idx]/softmax_sum[i];
        }
      }
      delete [] ones;
    }
  }
}

bool NN::Derivative(double* target, int batchsize) {

  for(int layer=layersizes.size()-1;layer>=1;layer--){
    int layersize = layersizes[layer];
    double* factor = error_matrixs[layer-1];
    double* delta = delta_matrixs[layer-1];
    if(layer == layersizes.size()-1) {
      for(int i=0;i<batchsize;i++) {
        int t = (int)target[i];

        for(int j=0;j<layersize;j++) {
          factor[layersize*i + j] = 0;
        }

        factor[layersize*i + t]=1;
      }

      double* output = layer_values.back();//minibatchsize *2
      int N = batchsize * layersize;
      double alpha = -1;
      cblas_daxpy(N, alpha, output, 1, factor, 1);

      double* X = layer_values[layer-1];
      int hidelayer_size = layersizes[layer-1];
      int weight_idx = layer-1;
      const int M = layersize;
      N = hidelayer_size;
      const int K = batchsize;
      const int lda = M;
      const int ldb = N;
      const int ldc = N;
      cblas_dgemm(CblasRowMajor, CblasTrans, CblasNoTrans,
                M, N, K,
                1.0, factor, lda,
                X, ldb,
                0.0, delta, ldc
                );
    } else {

      int weight_idx = layer-1;
      int downstream_layersize = layersizes[layer+1];
      double* downstream_factor = error_matrixs[weight_idx+1];//下游的
      double* downstream_weight = weight_matrixs[weight_idx+1];
      int M = batchsize;
      int N = layersize;
      int K = downstream_layersize;
      int lda = K;
      int ldb = K;
      int ldc = N;

      cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasTrans,
                  M, N, K,
                  -1.0, downstream_factor, lda,
                  downstream_weight, ldb,
                  0.0, factor, ldc
                 );//把下游的误差，通过权值累加到这一层
      double* O = layer_values[layer];

      for(int i=0;i<batchsize;i++) {
        for(int j=0;j<layersize;j++) {
          int idx=i*layersize+j;
          factor[idx]=factor[idx]*O[idx]*(1-O[idx]);
        }
      }
      double* X = layer_values[layer-1];
      int hidelayer_size = layersizes[layer-1];
      M = layersize;
      N = hidelayer_size;
      K = batchsize;
      lda = M;
      ldb = N;
      ldc = N;

      cblas_dgemm(CblasRowMajor, CblasTrans, CblasNoTrans,
                  M, N, K,
                  -1.0, factor, lda,
                  X, ldb,
                  0.0, delta, ldc
                  );
    }

    int hidelayer_size=layersizes[layer-1];
    int weight_idx = layer - 1;
    cblas_daxpy(layersize*hidelayer_size, learning_rate, delta, 1,
                weight_matrixs[weight_idx], 1
               );
  }
  return true;
}

bool NN::Train(double* feature, double* target, int instancenum) {
  int epoch = 0;
  while(true) {
    std::cout<<"epoch:"<<epoch++<<std::endl;
    for(int i=0;i<instancenum;i+=minibatchsize) {
      if(i%1000==0) {
        std::cout<<i<<"/"<<instancenum<<std::endl;
        std::cout.flush();
      }
      int batchsize =
          i+minibatchsize<instancenum ? minibatchsize : instancenum - i;
      double* feature_start_ptr = feature + i*inputsize;
      double* target_start_ptr = target + i;

      Forward(feature_start_ptr, batchsize);
      Derivative(target_start_ptr, batchsize);
    }
    double logloss;
    LogLoss(feature, target, logloss, instancenum);
    std::cout<<"LogLoss:"<<logloss<<std::endl;
  }
  return true;
}
/*
void NN::CompareWithTorch() {
  std::ifstream weight_fin("data/weight.txt");
  std::ifstream input_fin("data/input.txt");
  std::ifstream target_fin("data/target.txt");
  std::string line;
  std::vector<std::string> parts;
  //std::cout<<"minibatchsize:"<<minibatchsize<<std::endl;
  double* input = new double[minibatchsize*inputsize];
  double* target = new double[minibatchsize];
  for(int i=0;i<minibatchsize;i++) {
    getline(input_fin, line);
    boost::trim(line);
    boost::split(parts, line, boost::is_any_of(" "));
    if(parts.size()!=inputsize) {
      //LOG(ERROR)<<"input file error";
    }
    for(int j=0;j<inputsize;j++) {
      int idx = i * inputsize + j;
      input[idx] = boost::lexical_cast<double>(parts[j]);
    }
  }
   for(int i=0;i<minibatchsize;i++) {
    getline(target_fin, line);
    boost::trim(line);
    target[i] = boost::lexical_cast<double>(line);
  }

  std::cout<<"load weight"<<std::endl;
  for(int layer=1;layer<layersizes.size();layer++) {
    double* weight = weight_matrixs[layer-1];
    int weight_length = layersizes[layer-1] * layersizes[layer];
    for(int j=0;j<weight_length;j++) {
      getline(weight_fin, line);
      weight[j] = boost::lexical_cast<double>(line);
    }
  }
  weight_fin.close();
  input_fin.close();
  target_fin.close();
  Forward(input);
  double* output = GetOutput();
  std::cout<<"Compare With Torch"<<std::endl;
  for(int i=0;i<GetMiniBatchSize();i++) {
    for(int j=0;j<GetOutputSize();j++) {
      int idx = i*GetOutputSize() + j;
      std::cout<<output[idx]<<" ";
    }
    std::cout<<std::endl;
  }
  //  LogLoss(target);
  for(int i=0;i<10000;i++) {
    Forward(input);
    Derivative(target);
  }
}
*/

bool NN::LogLoss(double* feature, double* target, double &logloss, int instancenum) {
  //call LogLoss after calling Forward
  logloss = 0;
  double* logloss_tmp = new double[1];
  for(int i=0;i<instancenum;i+=minibatchsize) {
    int batchsize =
        i+minibatchsize<instancenum ? minibatchsize : instancenum - i;
    double* feature_start_ptr = feature + i*inputsize;
    double* target_start_ptr = target + i;

    Forward(feature_start_ptr, batchsize);
    double* y  = layer_values.back();
    for(int b=0;b<batchsize;b++) {
//      std::cout<<std::endl;
//      std::cout<<target_start_ptr[b]<<std::endl;
      for(int j=0;j<outputsize;j++) {
//        std::cout<<y[b*outputsize+j]<<" ";
        y[b*outputsize+j] = log(y[b*outputsize+j]);
      }
//      std::cout<<std::endl;
    }
    double* factor = error_matrixs.back();//借用一下这个空间
    for(int i=0;i<batchsize;i++) {
        int t = (int)target_start_ptr[i];
        for(int j=0;j<outputsize;j++) {
          factor[outputsize*i + j] = 0;
        }
        factor[outputsize*i + t]=1;
      }


    const enum CBLAS_ORDER Order = CblasRowMajor;
    const enum CBLAS_TRANSPOSE TransT = CblasNoTrans;
    const enum CBLAS_TRANSPOSE TransO = CblasTrans;
    const int M = 1;
    const int N = 1;
    const int K = batchsize * outputsize;
    const double alpha = 1;
    const double beta = 0;
    const int lda = K;
    const int ldb = K;
    const int ldc = N;
    cblas_dgemm(Order, TransT, TransO,
                M, N, K,
                alpha, factor, lda,
                y, ldb,
                beta, logloss_tmp, ldc
               );
      logloss+=logloss_tmp[0];
  }
  logloss/=instancenum;
  logloss = -logloss;
  delete [] logloss_tmp;
  return true;
}

