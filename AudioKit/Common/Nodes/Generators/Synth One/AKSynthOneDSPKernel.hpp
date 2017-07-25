//
//  AKSynthOneDSPKernel.hpp
//  AudioKit
//
//  Created by Aurelius Prochazka, revision history on Github.
//  Copyright © 2017 Aurelius Prochazka. All rights reserved.
//

#pragma once

#import "AKBankDSPKernel.hpp"

enum {
    index1Address = 0,
    index2Address = 1,
    morphBalanceAddress = 2,
    morph1SemitoneOffsetAddress = 3,
    morph2SemitoneOffsetAddress = 4,
    morph1VolumeAddress = 5,
    morph2VolumeAddress = 6,
    subVolumeAddress = 7,
    subOctaveDownAddress = 8,
    subIsSquareAddress = 9,
    fmVolumeAddress = 10,
    fmAmountAddress = 11,
    noiseVolumeAddress = 12,
    lfoIndexAddress = 13,
    lfoAmplitudeAddress = 14,
    lfoRateAddress = 15,
    cutoffAddress = 16,
    resonanceAddress = 17,
    filterMixAddress = 18,
    filterADSRMixAddress = 19,
    isMonoAddress = 20,
    glideAddress = 21,
    filterAttackDurationAddress = 22,
    filterDecayDurationAddress = 23,
    filterSustainLevelAddress = 24,
    filterReleaseDurationAddress = 25,
    attackDurationAddress = 26,
    decayDurationAddress = 27,
    sustainLevelAddress = 28,
    releaseDurationAddress = 29,
    detuningOffsetAddress = 30,
    detuningMultiplierAddress = 31,
    masterVolumeAddress = 32
};

class AKSynthOneDSPKernel : public AKBankDSPKernel, public AKOutputBuffered {
public:
    // MARK: Types
    struct NoteState {
        NoteState* next;
        NoteState* prev;
        AKSynthOneDSPKernel* kernel;
        
        enum { stageOff, stageOn, stageRelease };
        int stage = stageOff;
        
        float internalGate = 0;
        float amp = 0;
        float filter = 0;
        
        sp_adsr *adsr;
        sp_adsr *fadsr;
        sp_oscmorph *oscmorph1;
        sp_oscmorph *oscmorph2;
        sp_crossfade *morphCrossFade;
        sp_osc *subOsc;
        sp_fosc *fmOsc;
        
        sp_noise *noise;
        
        sp_moogladder *moog;
        sp_crossfade *filterCrossFade;
        
        void init() {
            sp_adsr_create(&adsr);
            sp_adsr_init(kernel->sp, adsr);
            sp_adsr_create(&fadsr);
            sp_adsr_init(kernel->sp, fadsr);
            
            sp_oscmorph_create(&oscmorph1);
            sp_oscmorph_init(kernel->sp, oscmorph1, kernel->ft_array, 4, 0);
            oscmorph1->freq = 0;
            oscmorph1->amp = 0;
            oscmorph1->wtpos = 0;
            
            sp_oscmorph_create(&oscmorph2);
            sp_oscmorph_init(kernel->sp, oscmorph2, kernel->ft_array, 4, 0);
            oscmorph2->freq = 0;
            oscmorph2->amp = 0;
            oscmorph2->wtpos = 0;
            
            sp_crossfade_create(&morphCrossFade);
            sp_crossfade_init(kernel->sp, morphCrossFade);
            
            sp_crossfade_create(&filterCrossFade);
            sp_crossfade_init(kernel->sp, filterCrossFade);
            
            sp_osc_create(&subOsc);
            sp_osc_init(kernel->sp, subOsc, kernel->sine, 0.0);
            
            sp_fosc_create(&fmOsc);
            sp_fosc_init(kernel->sp, fmOsc, kernel->sine);
            
            sp_noise_create(&noise);
            sp_noise_init(kernel->sp, noise);
            
            sp_moogladder_create(&moog);
            sp_moogladder_init(kernel->sp, moog);
        }
        

        void clear() {
            stage = stageOff;
            amp = 0;
        }
        
        // linked list management
        void remove() {
            if (prev) prev->next = next;
            else kernel->playingNotes = next;
            
            if (next) next->prev = prev;
            
            //prev = next = nullptr; Had to remove due to a click, potentially bad
            
            --kernel->playingNotesCount;
            sp_oscmorph_destroy(&oscmorph1);
            sp_oscmorph_destroy(&oscmorph2);
            sp_crossfade_destroy(&morphCrossFade);
            sp_crossfade_destroy(&filterCrossFade);
            sp_noise_destroy(&noise);
            sp_osc_destroy(&subOsc);
            sp_fosc_destroy(&fmOsc);
            sp_moogladder_destroy(&moog);
        }
        
        void add() {
            init();
            prev = nullptr;
            next = kernel->playingNotes;
            if (next) next->prev = this;
            kernel->playingNotes = this;
            ++kernel->playingNotesCount;
        }
        
        void noteOn(int noteNumber, int velocity) {
            noteOn(noteNumber, velocity, (float)noteToHz(noteNumber));
        }
        
        void noteOn(int noteNumber, int velocity, float frequency) {
            if (velocity == 0) {
                if (stage == stageOn) {
                    stage = stageRelease;
                    internalGate = 0;
                }
            } else {
                if (stage == stageOff) { add(); }
                oscmorph1->freq = (float)noteToHz(noteNumber + (int)kernel->morph1SemitoneOffset);
                oscmorph1->amp = (float)pow2(velocity / 127.);
                oscmorph2->freq = (float)noteToHz(noteNumber + (int)kernel->morph2SemitoneOffset);
                oscmorph2->amp = (float)pow2(velocity / 127.);
                subOsc->freq = (float)noteToHz(noteNumber);
                subOsc->amp = (float)pow2(velocity / 127.);
                
                fmOsc->freq = (float)noteToHz(noteNumber);
                fmOsc->amp = (float)pow2(velocity / 127.);
                
                noise->amp = (float)pow2(velocity / 127.);
                stage = stageOn;
                internalGate = 1;
            }
        }
        
        
        void run(int frameCount, float *outL, float *outR)
        {
            float originalFrequency1 = oscmorph1->freq;
            oscmorph1->freq *= kernel->detuningMultiplierSmooth;
            oscmorph1->freq = clamp(oscmorph1->freq, 0.0f, 22050.0f);
            oscmorph1->wtpos = kernel->index1;
            
            float originalFrequency2 = oscmorph2->freq;
            oscmorph2->freq *= kernel->detuningMultiplierSmooth;
            oscmorph2->freq += kernel->detuningOffset;
            oscmorph2->freq = clamp(oscmorph2->freq, 0.0f, 22050.0f);
            oscmorph2->wtpos = kernel->index2;
            
            float originalFrequencySub = subOsc->freq;
            subOsc->freq *= kernel->detuningMultiplierSmooth / (2.0 *
            (1.0 + kernel->subOctaveDown));
            
            float originalFrequencyFM = fmOsc->freq;
            fmOsc->freq *= kernel->detuningMultiplierSmooth;
            fmOsc->indx = kernel->fmAmount;
            
            adsr->atk = (float)kernel->attackDuration;
            adsr->dec = (float)kernel->decayDuration;
            adsr->sus = (float)kernel->sustainLevel;
            adsr->rel = (float)kernel->releaseDuration;
            
            fadsr->atk = (float)kernel->filterAttackDuration;
            fadsr->dec = (float)kernel->filterDecayDuration;
            fadsr->sus = (float)kernel->filterSustainLevel;
            fadsr->rel = (float)kernel->filterReleaseDuration;
            
            morphCrossFade->pos = kernel->morphBalanceSmooth;
            filterCrossFade->pos = kernel->filterMix;
            moog->res = kernel->resonanceSmooth;
            
            for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
                float oscmorph1_out = 0;
                float oscmorph2_out = 0;
                float osc_morph_out = 0;
                float subOsc_out = 0;
                float fmOsc_out = 0;
                float noise_out = 0;
                float filterOut = 0;
                float finalOut = 0;
                sp_adsr_compute(kernel->sp, adsr, &internalGate, &amp);
                sp_adsr_compute(kernel->sp, fadsr, &internalGate, &filter);
                
                //                filter *= kernel->filterADSRMix;
                
                moog->freq = kernel->cutoffSmooth + kernel->lfoOutput; // basic frequency
                moog->freq = moog->freq - moog->freq * kernel->filterADSRMix * (1.0 - filter);
                
                if (moog->freq < 0.0) {
                    moog->freq = 0.0;
                }
                
                sp_oscmorph_compute(kernel->sp, oscmorph1, nil, &oscmorph1_out);
                oscmorph1_out *= kernel->morph1Volume;
                sp_oscmorph_compute(kernel->sp, oscmorph2, nil, &oscmorph2_out);
                oscmorph2_out *= kernel->morph2Volume;
                sp_crossfade_compute(kernel->sp, morphCrossFade, &oscmorph1_out, &oscmorph2_out, &osc_morph_out);
                sp_osc_compute(kernel->sp, subOsc, nil, &subOsc_out);
                if (kernel->subIsSquare) {
                    if (subOsc_out > 0) {
                        subOsc_out = kernel->subVolume;
                    } else {
                        subOsc_out = -kernel->subVolume;
                    }
                } else {
                    subOsc_out *= kernel->subVolume * 2.0; // the 2.0 is to match square's volume
                }
                sp_fosc_compute(kernel->sp, fmOsc, nil, &fmOsc_out);
                fmOsc_out *= kernel->fmVolume;
                sp_noise_compute(kernel->sp, noise, nil, &noise_out);
                noise_out *= kernel->noiseVolume;
                
                float synthOut = amp * (osc_morph_out + subOsc_out + fmOsc_out + noise_out);
                
                sp_moogladder_compute(kernel->sp, moog, &synthOut, &filterOut);
                sp_crossfade_compute(kernel->sp, filterCrossFade, &synthOut, &filterOut, &finalOut);
                
                *outL++ += finalOut;
                *outR++ += finalOut;
                
            }
            oscmorph1->freq = originalFrequency1;
            oscmorph2->freq = originalFrequency2;
            subOsc->freq = originalFrequencySub;
            fmOsc->freq = originalFrequencyFM;
            if (stage == stageRelease && amp < 0.00001) {
                clear();
                remove();
            }
        }
        
    };

    // MARK: Member Functions

    AKSynthOneDSPKernel() {
        noteStates.resize(128);
        for (NoteState& state : noteStates) {
            state.kernel = this;
        }
    }

    void init(int _channels, double _sampleRate) override {
        AKBankDSPKernel::init(_channels, _sampleRate);
        sp_ftbl_create(sp, &sine, 2048);
        sp_gen_sine(sp, sine);
        
        sp_phasor_create(&lfo);
        sp_phasor_init(sp, lfo, 0);
        
        sp_port_create(&midiNotePort);
        sp_port_init(sp, midiNotePort, 0.0);
        
        sp_port_create(&multiplierPort);
        sp_port_init(sp, multiplierPort, 0.02);
        sp_port_create(&balancePort);
        sp_port_init(sp, balancePort, 0.1);
        sp_port_create(&cutoffPort);
        sp_port_init(sp, cutoffPort, 0.05);
        sp_port_create(&resonancePort);
        sp_port_init(sp, resonancePort, 0.05);
        
        sp_bitcrush_create(&bitcrush);
        sp_bitcrush_init(sp, bitcrush);

        sp_osc_create(&panOscillator);
        sp_osc_init(sp, panOscillator, sine, 0.0);
        sp_pan2_create(&pan);
        sp_pan2_init(sp, pan);
        
    }
    
    void setupMono() {
        
        sp_adsr_create(&adsr);
        sp_adsr_init(sp, adsr);
        sp_adsr_create(&fadsr);
        sp_adsr_init(sp, fadsr);
        
        sp_oscmorph_create(&oscmorph1);
        sp_oscmorph_init(sp, oscmorph1, ft_array, 4, 0);
        oscmorph1->freq = 0;
        oscmorph1->amp = 0;
        oscmorph1->wtpos = 0;
        
        sp_oscmorph_create(&oscmorph2);
        sp_oscmorph_init(sp, oscmorph2, ft_array, 4, 0);
        oscmorph2->freq = 0;
        oscmorph2->amp = 0;
        oscmorph2->wtpos = 0;
        
        sp_crossfade_create(&morphCrossFade);
        sp_crossfade_init(sp, morphCrossFade);
        
        sp_crossfade_create(&filterCrossFade);
        sp_crossfade_init(sp, filterCrossFade);
        
        sp_osc_create(&subOsc);
        sp_osc_init(sp, subOsc, sine, 0.0);
        
        sp_fosc_create(&fmOsc);
        sp_fosc_init(sp, fmOsc, sine);
        
        sp_noise_create(&noise);
        sp_noise_init(sp, noise);
        
        sp_moogladder_create(&moog);
        sp_moogladder_init(sp, moog);
        
        isMonoSetup = true;
        
    }
    
    void setupWaveform(uint32_t waveform, uint32_t size) {
        tbl_size = size;
        sp_ftbl_create(sp, &ft_array[waveform], tbl_size);
    }
    
    void setWaveformValue(uint32_t waveform, uint32_t index, float value) {
        ft_array[waveform]->tbl[index] = value;
    }
    
    void reset() {
        for (NoteState& state : noteStates) {
            state.clear();
        }
        playingNotes = nullptr;
        index1Ramper.reset();
        AKBankDSPKernel::reset();
    }
    
    standardBankKernelFunctions()

    void setParameters(float params[]) {
        for (int i = 0; i < 32; i++) {
            parameters[i] = params[i];
        }
    }

    void setParameter(AUParameterAddress address, AUValue value) {
        rampers[address].setUIValue(clamp(value, -100000.0f, 100000.0f)); // AOP Clamping is wrong
    }

    AUValue getParameter(AUParameterAddress address) {
        return rampers[address].getUIValue();
    }

    void startRamp(AUParameterAddress address, AUValue value, AUAudioFrameCount duration) override {
        rampers[address].startRamp(clamp(value, -100000.0f, 1000000.0f), duration); // AOP clamping is wrong
    }
    
    standardHandleMIDI()

    void handleTempoSetting(float currentTempo) {
        if (currentTempo != tempo) {
//            setParameter(cutoffAddress, currentTempo);
            tempo = currentTempo;
        }
    }

    void process(AUAudioFrameCount frameCount, AUAudioFrameCount bufferOffset) override {
        
        float* outL = (float*)outBufferListPtr->mBuffers[0].mData + bufferOffset;
        float* outR = (float*)outBufferListPtr->mBuffers[1].mData + bufferOffset;

        for (int i = 0; i < 33 ; i++ ) {
            parameters[i] = double(rampers[i].getAndStep());
        }
        index1 = parameters[index1Address];
        index2 = parameters[index2Address];
        morphBalance = parameters[morphBalanceAddress];
        morph1SemitoneOffset = parameters[morph1SemitoneOffsetAddress];
        morph2SemitoneOffset = parameters[morph2SemitoneOffsetAddress];
        morph1Volume = parameters[morph1VolumeAddress];
        morph2Volume = parameters[morph2VolumeAddress];
        subVolume = parameters[subVolumeAddress];
        subOctaveDown = parameters[subOctaveDownAddress];
        subIsSquare = parameters[subIsSquareAddress];
        fmVolume = parameters[fmVolumeAddress];
        fmAmount = parameters[fmAmountAddress];
        noiseVolume = parameters[noiseVolumeAddress];
        lfoIndex = parameters[lfoIndexAddress];
        lfoAmplitude = parameters[lfoAmplitudeAddress];
        lfoRate = parameters[lfoRateAddress];
        cutoff = parameters[cutoffAddress];
        resonance = parameters[resonanceAddress];
        filterMix = parameters[filterMixAddress];
        filterADSRMix = parameters[filterADSRMixAddress];
        isMono = parameters[isMonoAddress];
        glide = parameters[glideAddress];
        filterAttackDuration = parameters[filterAttackDurationAddress];
        filterDecayDuration = parameters[filterDecayDurationAddress];
        filterSustainLevel = parameters[filterSustainLevelAddress];
        filterReleaseDuration = parameters[filterReleaseDurationAddress];
        attackDuration = parameters[attackDurationAddress];
        decayDuration = parameters[decayDurationAddress];
        sustainLevel = parameters[sustainLevelAddress];
        releaseDuration = parameters[releaseDurationAddress];
        detuningOffset = parameters[detuningOffsetAddress];
        detuningMultiplier = parameters[detuningMultiplierAddress];
        masterVolume = parameters[masterVolumeAddress];

        for (AUAudioFrameCount i = 0; i < frameCount; ++i) {
            outL[i] = 0.0f;
            outR[i] = 0.0f;
        }
        
        if (isMono == 1) {
            NSLog(@"WARNING IS MONO");
            if (isMonoSetup == false) {
                setupMono();
            }
            
            //monoRun(frameCount, outL, outR);
            
        } else {
            
            NoteState* noteState = playingNotes;
            while (noteState) {
                noteState->run(frameCount, outL, outR);
                noteState = noteState->next;
            }
        }
        
        lfo->freq = lfoRate;
        
        for (AUAudioFrameCount i = 0; i < frameCount; ++i) {
            sp_phasor_compute(sp, lfo, nil, &lfoOutput);
            
            if (lfoShape == 0) {
                lfoOutput = sin(lfoOutput * M_PI * 2.0);
            } else if (lfoShape == 1) {
                if (lfoOutput > 0.5) {
                    lfoOutput = 1.0;
                } else {
                    lfoOutput = -1.0;
                }
            } else if (lfoShape == 2) {
                lfoOutput = (lfoOutput - 0.5) * 2.0;
            } else if (lfoShape == 3) {
                lfoOutput = (0.5 - lfoOutput) * 2.0;
            }
            
            lfoOutput *= lfoAmplitude;
            
            sp_port_compute(sp, multiplierPort, &detuningMultiplier, &detuningMultiplierSmooth);
            sp_port_compute(sp, balancePort, &morphBalance, &morphBalanceSmooth);
            sp_port_compute(sp, cutoffPort, &cutoff, &cutoffSmooth);
            sp_port_compute(sp, resonancePort, &resonance, &resonanceSmooth);
            float synthOut = outL[i];
            float finalOutL = 0.0;
            float finalOutR = 0.0;
            
//            sp_bitcrush_compute(sp, bitcrush, synthOut, finalOut)
            float panValue = 0.0;
            panOscillator->freq = 2.0;
            panOscillator->amp = 1.0;
            sp_osc_compute(sp, panOscillator, nil, &panValue);
            pan->pan = panValue;
            sp_pan2_compute(sp, pan, &synthOut, &finalOutL, &finalOutR);
            
            outL[i] = finalOutL * masterVolume;
            outR[i] = finalOutR * masterVolume;
//            outL[i] *= 0.5f;
//            outR[i] *= 0.5f;
        }
    }
    

    // MARK: Member Variables

private:
    std::vector<NoteState> noteStates;
    
    sp_ftbl *ft_array[4];
    UInt32 tbl_size = 4096;
    
    sp_ftbl *sine;
    sp_phasor *lfo;
    sp_bitcrush *bitcrush;
    sp_pan2 *pan;
    sp_osc *panOscillator;

    float lfoOutput = 0.0;
    
    sp_port *midiNotePort;
    float midiNote = 0.0;
    float midiNoteSmooth = 0.0;
    
    sp_port *multiplierPort;
    sp_port *balancePort;
    sp_port *cutoffPort;
    sp_port *resonancePort;

    float tempo = 0.0;
public:

    float parameters[33] = {
        0, // index1
        0, // index2
        0.5, // morphBalance
        0, // morph1SemitoneOffset
        0, // morph2SemitoneOffset
        1, // morph1Volume
        1, // morph2Volume
        0, // subVolume
        0, // subOctaveDown
        0, // subIsSquare
        0, // fmVolume
        0, // fmAmount
        0, // noiseMix
        0, // lfoIndex
        1000, // lfoAmplitude
        1, // lfoRate
        1000, // cutoff
        0.5, // resonance
        0.5, // filterMix
        0.5, // filterADSRMix
        0, // isMono
        0, // glide
        0.1, // filterAttackDuration
        0.1, // filterDecayDuration
        1, // filterSustainLevel
        0.1, // filterReleaseDuration
        0.1, // attackDuration
        0.1, // decayDuration
        0.1, // sustainLevel
        0.1, // releaseDuration
        0, // detuningOffset
        1, // detuningMultiplier
        0.8 // masterVolume
    };

    // Ported values
    float morphBalanceSmooth = 0.5666;
    float detuningMultiplierSmooth = 1.66;
    float cutoffSmooth = 1666;
    float resonanceSmooth = 0.5;

    // Orphan
    float lfoShape = 0;

    NoteState* playingNotes = nullptr;
    int playingNotesCount = 0;

    // MONO Stuff
    
    bool isMonoSetup = false;
    
    float internalGate = 0;
    float amp = 0;
    float filter = 0;
    
    sp_adsr *adsr;
    sp_adsr *fadsr;
    sp_oscmorph *oscmorph1;
    sp_oscmorph *oscmorph2;
    sp_crossfade *morphCrossFade;
    sp_osc *subOsc;
    sp_fosc *fmOsc;
    
    sp_noise *noise;
    
    sp_moogladder *moog;
    sp_crossfade *filterCrossFade;
    
    bool notesHeld = false;

    float index1 = parameters[index1Address];
    float index2 = parameters[index2Address];
    float morphBalance = parameters[morphBalanceAddress];
    float morph1SemitoneOffset = parameters[morph1SemitoneOffsetAddress];
    float morph2SemitoneOffset = parameters[morph2SemitoneOffsetAddress];
    float morph1Volume = parameters[morph1VolumeAddress];
    float morph2Volume = parameters[morph2VolumeAddress];
    float subVolume = parameters[subVolumeAddress];
    float subOctaveDown = parameters[subOctaveDownAddress];
    float subIsSquare = parameters[subIsSquareAddress];
    float fmVolume = parameters[fmVolumeAddress];
    float fmAmount = parameters[fmAmountAddress];
    float noiseVolume = parameters[noiseVolumeAddress];
    float lfoIndex = parameters[lfoIndexAddress];
    float lfoAmplitude = parameters[lfoAmplitudeAddress];
    float lfoRate = parameters[lfoRateAddress];
    float cutoff = parameters[cutoffAddress];
    float resonance = parameters[resonanceAddress];
    float filterMix = parameters[filterMixAddress];
    float filterADSRMix = parameters[filterADSRMixAddress];
    float isMono = parameters[isMonoAddress];
    float glide = parameters[glideAddress];
    float filterAttackDuration = parameters[filterAttackDurationAddress];
    float filterDecayDuration = parameters[filterDecayDurationAddress];
    float filterSustainLevel = parameters[filterSustainLevelAddress];
    float filterReleaseDuration = parameters[filterReleaseDurationAddress];
    float masterVolume = parameters[masterVolumeAddress];
    float attackDuration = parameters[attackDurationAddress];
    float decayDuration = parameters[decayDurationAddress];
    float sustainLevel = parameters[sustainLevelAddress];
    float releaseDuration = parameters[releaseDurationAddress];
    float detuningOffset = parameters[detuningOffsetAddress];
    float detuningMultiplier = parameters[detuningMultiplierAddress];

    ParameterRamper index1Ramper = parameters[index1Address];
    ParameterRamper index2Ramper = parameters[index2Address];
    ParameterRamper morphBalanceRamper = parameters[morphBalanceAddress];
    ParameterRamper morph1SemitoneOffsetRamper = parameters[morph1SemitoneOffsetAddress];
    ParameterRamper morph2SemitoneOffsetRamper = parameters[morph2SemitoneOffsetAddress];
    ParameterRamper morph1VolumeRamper = parameters[morph1VolumeAddress];
    ParameterRamper morph2VolumeRamper = parameters[morph2VolumeAddress];
    ParameterRamper subVolumeRamper = parameters[subVolumeAddress];
    ParameterRamper subOctaveDownRamper = parameters[subOctaveDownAddress];
    ParameterRamper subIsSquareRamper = parameters[subIsSquareAddress];
    ParameterRamper fmVolumeRamper = parameters[fmVolumeAddress];
    ParameterRamper fmAmountRamper = parameters[fmAmountAddress];
    ParameterRamper noiseVolumeRamper = parameters[noiseVolumeAddress];
    ParameterRamper lfoIndexRamper = parameters[lfoIndexAddress];
    ParameterRamper lfoAmplitudeRamper = parameters[lfoAmplitudeAddress];
    ParameterRamper lfoRateRamper = parameters[lfoRateAddress];
    ParameterRamper cutoffRamper = parameters[cutoffAddress];
    ParameterRamper resonanceRamper = parameters[resonanceAddress];
    ParameterRamper filterMixRamper = parameters[filterMixAddress];
    ParameterRamper filterADSRMixRamper = parameters[filterADSRMixAddress];
    ParameterRamper isMonoRamper = parameters[isMonoAddress];
    ParameterRamper glideRamper = parameters[glideAddress];
    ParameterRamper filterAttackDurationRamper = parameters[filterAttackDurationAddress];
    ParameterRamper filterDecayDurationRamper = parameters[filterDecayDurationAddress];
    ParameterRamper filterSustainLevelRamper = parameters[filterSustainLevelAddress];
    ParameterRamper filterReleaseDurationRamper = parameters[filterReleaseDurationAddress];
    ParameterRamper attackDurationRamper = parameters[attackDurationAddress];
    ParameterRamper decayDurationRamper =  parameters[decayDurationAddress];
    ParameterRamper sustainLevelRamper = parameters[sustainLevelAddress];
    ParameterRamper releaseDurationRamper = parameters[releaseDurationAddress];
    ParameterRamper detuningOffsetRamper = parameters[detuningOffsetAddress];
    ParameterRamper detuningMultiplierParameterRamper = parameters[detuningMultiplierAddress];
    ParameterRamper masterVolumeRamper = parameters[masterVolumeAddress];

    float parameterValues[33] = {
        index1,
        index2,
        morphBalance,
        morph1SemitoneOffset,
        morph2SemitoneOffset,
        morph1Volume,
        morph2Volume,
        subVolume,
        subOctaveDown,
        subIsSquare,
        fmVolume,
        fmAmount,
        noiseVolume,
        lfoIndex,
        lfoAmplitude,
        lfoRate,
        cutoff,
        resonance,
        filterMix,
        filterADSRMix,
        isMono,
        glide,
        filterAttackDuration,
        filterDecayDuration,
        filterSustainLevel,
        filterReleaseDuration,
        attackDuration,
        decayDuration,
        sustainLevel,
        releaseDuration,
        detuningOffset,
        detuningMultiplier,
        masterVolume
    };
    
    ParameterRamper rampers[33] = {
        index1Ramper,
        index2Ramper,
        morphBalanceRamper,
        morph1SemitoneOffsetRamper,
        morph2SemitoneOffsetRamper,
        morph1VolumeRamper,
        morph2VolumeRamper,
        subVolumeRamper,
        subOctaveDownRamper,
        subIsSquareRamper,
        fmVolumeRamper,
        fmAmountRamper,
        noiseVolumeRamper,
        lfoIndexRamper,
        lfoAmplitudeRamper,
        lfoRateRamper,
        cutoffRamper,
        resonanceRamper,
        filterMixRamper,
        filterADSRMixRamper,
        isMonoRamper,
        glideRamper,
        filterAttackDurationRamper,
        filterDecayDurationRamper,
        filterSustainLevelRamper,
        filterReleaseDurationRamper,
        attackDurationRamper,
        decayDurationRamper,
        sustainLevelRamper,
        releaseDurationRamper,
        detuningOffsetRamper,
        detuningMultiplierRamper,
        masterVolume
    };
};

