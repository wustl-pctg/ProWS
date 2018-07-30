//------------------------------------------------------------------------
//      ____                        _      _
//     / ___|____ _   _ ____   ____| |__  | |
//    | |   / ___| | | |  _  \/ ___|  _  \| |
//    | |___| |  | |_| | | | | |___| | | ||_|
//     \____|_|  \_____|_| |_|\____|_| |_|(_) Media benchmarks
//                           
//	  2006, Intel Corporation, licensed under Apache 2.0 
//
//  file :	 ParticleFilterCilk.h
//  author : Scott Ettinger - scott.m.ettinger@intel.com
//
//  description : OpenMP parallelized version of the particle filter
//					object derived from ParticleFilter.h
//		
//  modified : 
//--------------------------------------------------------------------------

#ifndef PARTICLEFILTERCILK_H
#define PARTICLEFILTERCILK_H

#include <cilk/cilk.h>
#include "ParticleFilter.h"

template<class T> 
class ParticleFilterCilk : public ParticleFilter<T> {

    using ParticleFilter<T>:: mModel;
    using ParticleFilter<T>:: mWeights;
    using ParticleFilter<T>:: mParticles;
    using ParticleFilter<T>:: mNewParticles;
    using ParticleFilter<T>:: mBestParticle;
    using ParticleFilter<T>:: mNParticles;
    using ParticleFilter<T>:: mMinParticles;
    using ParticleFilter<T>:: mBins;  
    using ParticleFilter<T>:: mRnd;
    using ParticleFilter<T>:: mInitialized; // Cilk only
    using ParticleFilter<T>:: mCdf; // Cilk only
    using ParticleFilter<T>:: mSamples; // Cilk only
    typedef typename ParticleFilter<T>::fpType fpType;
    typedef typename ParticleFilter<T>::Vectorf Vectorf;

protected:
    std::vector<int> mIndex; //list of particles to regenerate

    //calculate particle weights - threaded version 
    //calculate particle weights based on model likelihood
    void CalcWeights(std::vector<Vectorf > &particles);

    //New particle generation - threaded version 
    void GenerateNewParticles(int k);

public:
    //Particle filter update
    bool Update(fpType timeval);
};

//Calculate particle weights (mWeights) and find highest likelihood particle. 
//computes an optimal annealing factor and scales the likelihoods. 
template<class T>
void ParticleFilterCilk<T>::CalcWeights(std::vector<Vectorf > &particles)
{
    std::vector<unsigned char> valid(particles.size());
    mBestParticle = 0;
    fpType total = 0, best = 0, minWeight = 1e30f, annealingFactor = 1;
    mWeights.resize(particles.size());

    int np = (int)particles.size();

    //OpenMP parallelized loop to compute log-likelihoods
    #pragma cilk grainsize = 32
    cilk_for(int j = 0; j < np; j++) 
    {
        bool vflag;
        int local_j = j;
        //compute log-likelihood weights for each particle
        mWeights[local_j] = mModel->LogLikelihood(particles[j], vflag, local_j);
        valid[local_j] = vflag ? 1 : 0;
    }

    uint i = 0;
    while(i < particles.size())
    {	
        if(!valid[i]) //if not valid(model prior), remove the particle from the list
        {	
            particles[i] = particles[particles.size() - 1];
            mWeights[i] = mWeights[particles.size() - 1];
            valid[i] = valid[valid.size() - 1];
            particles.pop_back(); mWeights.pop_back(); valid.pop_back();
        }
        else
        {
            minWeight = std::min(mWeights[i++], minWeight); //find minimum log-likelihood
        }
    }
    //bail out if not enough valid particles
    if((int)particles.size() < mMinParticles) return;

    mWeights -= minWeight; //shift weights to zero for numerical stability
    if(mModel->StdDevs().size() > 1) 
    {
        //calculate annealing factor if more than 1 step
        annealingFactor = BetaAnnealingFactor(mWeights, 0.5f);
    }

    for(i = 0; i < mWeights.size(); i++)
    {	
        double wa = annealingFactor * mWeights[i];
        //exponentiate log-likelihoods scaled by annealing factor
        mWeights[i] = (float)exp(wa);
        total += mWeights[i]; //save sum of all weights
        if(i == 0 || mWeights[i] > best) //find highest likelihood particle
        {	
            best = mWeights[i];
            mBestParticle = i;
        }
    }
    mWeights *= fpType(1.0) / total; //normalize weights
}


//generate new particles distributed with std deviation given 
//by the model annealing parameter - threaded
template<class T> 
void ParticleFilterCilk<T>::GenerateNewParticles(int k)
{
    int p = 0;
    mNewParticles.resize(mNParticles);
    mIndex.resize(mNParticles);
    for(int i = 0; i < (int)mBins.size(); i++) {
        for(uint j = 0; j < mBins[i]; j++) {
            //index particles to be regenerated
            mIndex[p++] = i;
        }
    }

    //distribute new particles randomly according to model stdDevs
    #pragma cilk grainsize = 32
    cilk_for(int i = 0; i < mNParticles; i++)
    {	
        //add new particle for each entry in each bin distributed randomly 
        //about duplicated particle
        mNewParticles[i] = mParticles[mIndex[i]];
        this->AddGaussianNoise(mNewParticles[i], mModel->StdDevs()[k], mRnd[i]);
    }
}

//Particle filter update (model and observation updates must be called first)  
//weights have already been computed from previous step or initialization
//The call to TrackingModel::GetObservation has been moved out of this
//function and called in the outer loop for processing a frame instead.
//This way we can create a pipeline, where the GetObservation is done as the
//first stage and Update as the second stage.
template<class T> bool 
ParticleFilterCilk<T>::Update(fpType timeval)
{						
    if(!mInitialized) //check for proper initialization
    {	
        std::cout << "Update Error : Particles not initialized" << std::endl; 
        return false;
    }	

    //loop over all annealing steps starting with highest
    for(int k = (int)mModel->StdDevs().size() - 1; k >= 0 ; k--)
    {
        this->CalcCDF(mWeights, mCdf); //Monte Carlo re-sampling 
        this->Resample(mCdf, mBins, mSamples, mNParticles);		
        bool minValid = false;
        while(!minValid)
        {
            this->GenerateNewParticles(k);
            //calculate particle weights and remove any invalid particles
            this->CalcWeights(mNewParticles); 
            //repeat if not enough valid particles
            minValid = (int)mNewParticles.size() >= mMinParticles;
            if(!minValid) 
                std::cout << "Not enough valid particles - Resampling!!!" << std::endl;
        }
        mParticles = mNewParticles; //save new particle set
    }

    return true;
}
#endif

