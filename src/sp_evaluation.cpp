#ifndef SPEVALUATION
#define SPEVALUATION

#include "sp_evaluation.h"

SpEvaluation::SpEvaluation(){}

void SpEvaluation::init(int data_size, int dim_size){
    this->data_size = data_size;
    this->dim_size = dim_size;
    samples_size = (int) sqrt(data_size * 9.0);
    samples = new int[samples_size];
    first_position_offset_sum = 0;
    last_position = new float[samples_size * dim_size];

}

void SpEvaluation::generate_samples(){
    genrandom rand;
    rand.init_gsl();
    //std::mt19937::result_type seed = time(0);
    //auto dice_rand = std::bind(std::uniform_int_distribution<int>(0, data_size-1), std::mt19937(seed));
    for(int i = 0; i < samples_size; i++){
        samples[i] = (floor)(rand.gslRandom() * (data_size - 0.1));
    }
}

void SpEvaluation::set_init_position(float *data){
    for(int sample_index = 0; sample_index < samples_size; sample_index++){
        int data_index = samples[sample_index];
        for(int dim_index = 0; dim_index < dim_size; dim_index++){
            last_position[sample_index * dim_size + dim_index] = data[data_index * dim_size + dim_index];
        }
    }
}

float SpEvaluation::current_position_offset(float *data){
    float sum = 0;
#pragma omp parallel for reduction(+:sum)
    for(int sample_index = 0; sample_index < samples_size; sample_index++){
        int data_index = samples[sample_index];
        for(int dim_index = 0; dim_index < dim_size; dim_index++){
            float offset  = last_position[sample_index * dim_size + dim_index] - data[data_index * dim_size + dim_index];
            last_position[sample_index * dim_size + dim_index] = data[data_index * dim_size + dim_index];
            sum += offset * offset;
        }
    }
    return sum;
}

float SpEvaluation::start_evaluation(float *data){
    if(index == 0){
        set_init_position(data);
        index++;
        return 0;
    }else if(index == 1){
        first_position_offset_sum = current_position_offset(data);
        std::cout<<"StartEval: " << first_position_offset_sum <<std::endl;
        start_offset[(index-1)%5] = first_position_offset_sum;
        index++;
        return 0;
    }else{
        int length = index - 1;
        if (index > 5)
            length = 5;
        float last_ave_offset = 0;
        for(int i=0; i< length; i++){
            last_ave_offset += start_offset[i];
        }
        last_ave_offset = last_ave_offset / length;

        last_position_offset_sum = current_position_offset(data);
        start_offset[(index-1)%5] = last_position_offset_sum;
        length = index;
        if(index >= 5)
            length = 5;
        float ave_offset = 0;
        for(int i=0; i < length; i++){
            ave_offset += start_offset[i];
        }
        ave_offset = ave_offset / length;
        std::cout<< " StartEval: " << last_position_offset_sum << " ave:" << ave_offset << " percent:" << ave_offset / last_ave_offset << "%" <<std::endl;
        index++;
        return ave_offset / last_ave_offset;
    }

}

float SpEvaluation::stop_evaluation2(float **offset){
    stop_index++;
    float last_ave_offset = 0;
    float new_ave_offset = 0;
    if(stop_index > 5){
        for(int i = 0; i < 5; i++){
            last_ave_offset += stop_offset[i];
        }
        last_ave_offset = last_ave_offset / 5;
    }

    float sum=0;
#pragma omp parallel for reduction(+:sum)
    for(int i = 0; i < stop_samples_size; i++){
        for(int j =0; j < dim_size; j++){
            int ii = stop_samples[i];
            sum += (cluster_last_offset[ii * dim_size + j] - offset[ii][j]) *  (cluster_last_offset[ii * dim_size + j] -offset[ii][j]);
            cluster_last_offset[ii * dim_size + j] = offset[ii][j];
        }
    }
    stop_offset[(stop_index-1) % 5] = sum;
    if(stop_index > 5){
        for(int i = 0; i < 5; i++){
            new_ave_offset += stop_offset[i];
        }
        new_ave_offset = new_ave_offset / 5;
    }

    if(stop_index > 5){
        std::cout<< " StopEval:" << sum << " ave:"<< new_ave_offset << " percent: " << new_ave_offset / last_ave_offset <<std::endl;
        return new_ave_offset / last_ave_offset;
    }else{
        return 0;
    }

}


float SpEvaluation::stop_evaluation(float **offset){

    //std::vector<Dis> distance;
    //int i = 10;
    //for(int j = 0; j <cluster_size; j++){
    //   float dis = 0;
    //    for(int d = 0; d < dim_size; d++){
    //        dis += (centers[i][d] + offset[i][d]- centers[j][d] - offset[j][d])
    //            *(centers[i][d] + offset[i][d] - centers[j][d] - offset[j][d]);
    //    }
    //    //dis = 20 - j;
    //    distance.push_back(Dis(i,j,dis));
    //}
    //std::sort(distance.begin(), distance.end());
    //std::cout<< centers[100][0] << std::endl;
    //for(int i = 0 ;i < 10; i++){
    //    std::cout<< distance[i].index_to << " ";
    //}
    //std::cout<< "" <<std::endl;
   // std::cout<< distance[0].index_to << " " << distance[1].index_to << " " << distance[2].index_to << " " << distance[3].index_to << std::endl;
    //std::cout<< distance[0].distance << " " << distance[1].distance << " " << distance[2].distance << " " << distance[3].distance << std::endl;
    //std::cout<< center.index << std::endl;
    stop_index++;
    float last_ave_offset = 0;
    float new_ave_offset = 0;
    if(stop_index > 5){
        for(int i = 0; i < 5; i++){
            last_ave_offset += stop_offset[i];
        }
        last_ave_offset = last_ave_offset / 5;
    }

    float sum=0;
    for(int i = 0; i < cluster_size; i++){
        for(int j =0; j < dim_size; j++){
            sum += (cluster_last_offset[i * dim_size + j] - offset[i][j]) *  (cluster_last_offset[i * dim_size + j] -offset[i][j]);
            cluster_last_offset[i * dim_size + j] = offset[i][j];
        }
    }
    stop_offset[(stop_index-1) % 5] = sum;
    if(stop_index > 5){
        for(int i = 0; i < 5; i++){
            new_ave_offset += stop_offset[i];
        }
        new_ave_offset = new_ave_offset / 5;
    }

    std::cout<< " StopEval:" << sum << " percent: " << new_ave_offset / last_ave_offset <<std::endl;
    if(stop_index > 5){
        return new_ave_offset / last_ave_offset;
    }else{
        return 0;
    }
}

void SpEvaluation::setStopParams(int cluster_size) {
    this->cluster_size = cluster_size;
    //stop_samples_size =  (int) sqrt(cluster_size * 9.0);
    //stop_samples = new int[stop_samples_size];
    cluster_last_offset = new float[cluster_size * dim_size];
    //generate_stopSamples();
}

void SpEvaluation::generate_stopSamples() {
    genrandom rand;
    rand.init_gsl();
    for(int i = 0; i < stop_samples_size; i++){
        stop_samples[i] = (floor)(rand.gslRandom() * (stop_samples_size - 0.1));
    }
}


#endif //SPEVALUATION
