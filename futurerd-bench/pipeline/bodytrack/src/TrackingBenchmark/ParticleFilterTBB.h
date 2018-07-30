 //------------------------------------------------------------------------
//      ____                        _      _
//     / ___|____ _   _ ____   ____| |__  | |
//    | |   / ___| | | |  _  \/ ___|  _  \| |
//    | |___| |  | |_| | | | | |___| | | ||_|
//     \____|_|  \_____|_| |_|\____|_| |_|(_) Media benchmarks
//                           
//	 © 2006, Intel Corporation, licensed under Apache 2.0 
//
//  file :	 ParticleFilterTBB.h
//  author : 
//
//  description : Intel TBB parallelized version of the particle filter
//                    object dervied from ParticleFilter.h
//  modified : 
//--------------------------------------------------------------------------

#ifndef PARTICLEFILTERTBB_H
#define PARTICLEFILTERTBB_H

#include "ParticleFilter.h"
#include "TBBtypes.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/parallel_for.h"
#include "tbb/parallel_reduce.h"
#include "tbb/blocked_range.h"
#include "tbb/pipeline.h"
#include <fstream>

#undef min

#define WORKUNIT_SIZE_NEWPARTICLES 32
#define WORKUNIT_SIZE_CALCWEIGHTS 32

template<class T> 
class ParticleFilterTBB : public ParticleFilter<T>, public tbb::filter {

    using ParticleFilter<T>:: mModel;
    using ParticleFilter<T>:: mWeights;
    using ParticleFilter<T>:: mParticles;
    using ParticleFilter<T>:: mNewParticles;
    using ParticleFilter<T>:: mBestParticle;
    using ParticleFilter<T>:: mNParticles;
    using ParticleFilter<T>:: mMinParticles;
    using ParticleFilter<T>:: mBins;  
    using ParticleFilter<T>:: mRnd;
    using ParticleFilter<T>:: mInitialized; // TBB only
    using ParticleFilter<T>:: mCdf; // TBB only
    using ParticleFilter<T>:: mSamples; // TBB only
    typedef typename ParticleFilter<T>::fpType fpType;
    typedef typename ParticleFilter<T>::Vectorf Vectorf;

private:	
    T* getModel() { return mModel; };

protected:
    std::vector<int> mIndex; //list of particles to regenerate
    
    std::ofstream mPoseOutFile; //output pose file; TBB only
    bool mOutputBMP; //write bitmap output flag; TBB only
    unsigned int mFrame; //current frame being processed; TBB only

    std::vector<unsigned char> mValid; //storage for valid particle flags

    //calculate particle weights - threaded version 
    //calculate particle weights based on model likelihood
    void CalcWeights(std::vector<Vectorf > &particles);	

    //New particle generation - threaded version 
    void GenerateNewParticles(int k);

    void WritePose(std::ostream &f, std::vector<float> &pose); // TBB only

    //TBB pipeline stage function
    void *operator()(void *token);

public:
    ParticleFilterTBB() : tbb::filter(serial), mOutputBMP(false), mFrame(0) {};
    virtual ~ParticleFilterTBB() { };

    //sets
    void setOutputFile(const char *fname) { mPoseOutFile.open(fname); };
    void setOutputBMP(bool flag) { mOutputBMP = flag; };

    //Particle filter update
    bool Update(fpType timeval);

protected:

    //-------------- TBB block computing objects ---------------------------

    //particle generation block computing object
    template<class S>
    class DoGenerateNewParticlesTBB {

        S *mModel;
        std::vector<Vectorf> &mNewParticles, &mParticles;
        std::vector<RandomGenerator> &mRnd; 
        std::vector<int> &mIndex;

        int k;
        //distribute particle randomly according to given standard deviations
        inline void 
        AddGaussianNoise(Vectorf &p, const Vectorf &stdDevs, RandomGenerator &rnd) const
        {	
            for(uint i = 0; i < stdDevs.size(); i++)
                p[i] += (fpType)rnd.RandN() * stdDevs[i];				
        }

    public: 
        void operator() ( const tbb::blocked_range<int> &r ) const 
        {	
            for( int i = r.begin(); i < r.end(); i++ )
            {
                mNewParticles[i] = mParticles[mIndex[i]];
                this->AddGaussianNoise(mNewParticles[i], mModel->StdDevs()[k], mRnd[i]);
            }
        }
        DoGenerateNewParticlesTBB(int _k, S* _model, 
                                  std::vector<Vectorf> &_mNewParticles,
                                  std::vector<Vectorf> &_mParticles, 
                                  std::vector<RandomGenerator> &_mRnd, 
                                  std::vector<int> &_mIndex) 
            : mModel(_model), mNewParticles(_mNewParticles), 
              mParticles(_mParticles), mRnd(_mRnd), mIndex(_mIndex), k(_k) {} 
    };

    //likelihood block computing object
    template<class S>
    class DoCalcLikelihoods {
    private:
        S *mModel;
        Vectorf *mParticles;
        fpType *mWeights;
        unsigned char *mValid;
    public:

        DoCalcLikelihoods(S *model, Vectorf *particles, fpType *weights, 
                unsigned char *valid) : 
            mModel(model), mParticles(particles), mWeights(weights), mValid(valid) {};

        void operator()( const tbb::blocked_range<int>& r ) const 
        {
            for( int i = r.begin(); i!=r.end(); ++i )
            {	
                bool vflag;
                //compute log-likelihood weights for each particle
                mWeights[i] = mModel->LogLikelihood(mParticles[i], vflag, i);
                mValid[i] = vflag ? 1 : 0;
            }
        }
    };
};

template<class T> 
void ParticleFilterTBB<T>::CalcWeights(std::vector<Vectorf> &particles) {
    mBestParticle =0; 
    fpType total =0, best =0, minWeight = 1e30f, annealingFactor = 1;
    mWeights.resize(particles.size());

    //p_particles = &particles; 
    //p_weights = &mWeights;//Returns pointer to mWeights
    int np = (int) particles.size(); 
    std::vector<unsigned char> &valid = mValid;
    valid.resize(np); //Resize global valid to num. particles
    uint i = 0;

    //parallel code to calculate likelihoods
    tbb::parallel_for(tbb::blocked_range<int>(0, np, 
            WORKUNIT_SIZE_CALCWEIGHTS), 
            DoCalcLikelihoods<T>(mModel, &particles[0], &mWeights[0], &valid[0])
    );

    i = 0;
    while(i < particles.size())
    {	
        if(!valid[i])//if not valid(model prior), remove the particle from the list
        {	
            particles[i] = particles[particles.size() - 1];
            mWeights[i] = mWeights[particles.size() - 1];
            valid[i] = valid[valid.size() - 1];
            particles.pop_back(); mWeights.pop_back(); valid.pop_back();
        }
        else
        {
            minWeight = std::min(mWeights[i++], minWeight); //find minimum weight
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
        total += mWeights[i];//save sum of all weights
        if(i == 0 || mWeights[i] > best) //find highest likelihood particle
        {	
            best = mWeights[i];
            mBestParticle = i;
        }
    }
    mWeights *= fpType(1.0) / total; //normalize weights
}

template<class T>
void ParticleFilterTBB<T>::GenerateNewParticles(int k)
{
    int p = 0;
    mNewParticles.resize(mNParticles);
    mIndex.resize(mNParticles);
    for(int i = 0; i < (int)mBins.size(); i++)
        for(uint j = 0; j < mBins[i]; j++)
            mIndex[p++] = i;

    //TBB parallel_for
    parallel_for(tbb::blocked_range<int>(0, mNParticles, 
        WORKUNIT_SIZE_NEWPARTICLES), 
        DoGenerateNewParticlesTBB<T>(k, getModel(), mNewParticles, mParticles, mRnd, mIndex)
    );
}

//Particle filter update (model and observation updates must be called first)  
//weights have already been computed from previous step or initialization
template<class T>
bool ParticleFilterTBB<T>::Update(fpType timeval)
{						
    if(!mInitialized) //check for proper initialization
    {	
        std::cout << "Update Error : Particles not initialized" << std::endl; 
        return false;
    }
    //loop over all annealing steps starting with highest
    for(int k = (int)mModel->StdDevs().size() - 1; k >= 0 ; k--)
    {	
        CalcCDF(mWeights, mCdf); //Monte Carlo re-sampling 
        Resample(mCdf, mBins, mSamples, mNParticles);		
        bool minValid = false;
        while(!minValid)
        {	
            GenerateNewParticles(k);
            //calculate particle weights and remove any invalid particles
            CalcWeights(mNewParticles); 
            //repeat if not enough valid particles
            minValid = (int)mNewParticles.size() >= mMinParticles;
            if(!minValid) 
                std::cout << "Not enough valid particles - Resampling!!!" << std::endl;
        }
        mParticles = mNewParticles; //save new particle set
    }

    return true;
}

//write a given pose to a stream
template<class T>
void ParticleFilterTBB<T>::WritePose(std::ostream &f, std::vector<float> &pose)
{	
    for(int i = 0; i < (int)pose.size(); i++)
        f << pose[i] << " ";
    f << std::endl;
}

template<class T>
void *ParticleFilterTBB<T>::operator ()(void *token)
{
    ImageSetToken *images = (ImageSetToken *)token; //TBB uses void * for passing tokens!
    mModel->SetObservation(*images);
    Update(0); //call particle filter update
    std::vector<float> estimate; //expected pose from particle distribution
    ParticleFilter<T>::Estimate(estimate); //get average pose of the particle distribution
    WritePose(mPoseOutFile, estimate);
    if(mOutputBMP)
        mModel->OutputBMP(estimate, mFrame++);								//save output bitmap file

    delete images;

    return NULL;
}


#endif
