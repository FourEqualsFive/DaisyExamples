#include "daisy_seed.h"
#include "daisysp.h"

// Use the daisy namespace to prevent having to type
// daisy:: before all libdaisy functions
using namespace daisy;
using namespace daisysp;

// Declare a DaisySeed object called hardware
DaisySeed hardware;

Metro tick;

Oscillator clickOsc;
Oscillator bloopOsc;
WhiteNoise noise;

AdEnv clickVolEnv, clickPitchEnv, tissEnv;
Adsr bloopPitchEnv, bloopVolEnv;
AnalogBassDrum bassEnv;
AnalogSnareDrum snareEnv;

AdcChannelConfig adcConfig;

Switch click, tiss, bass, snare;
Switch bloop;

bool bloopVolGate, bloopFreqGate;
const int base_f = 600;

void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    int nOsc = 0;
    float freq_set, freqMod;
    float bassOsc_out, snareOsc_out = 0;
    float clickOsc_out, clk_env_out;
    float bloopOsc_out, blp_env_out;
    float osc_out, noise_out, tss_env_out, sig;
    //Get rid of any bouncing
    tiss.Debounce();
    click.Debounce();
    bass.Debounce();
    snare.Debounce();
    bloop.Debounce();

    //If you press the click button...
    if(click.RisingEdge())
    {
        //Trigger both envelopes!
        clickVolEnv.Trigger();
        clickPitchEnv.Trigger();
    }

    //If you press the bloop button...
    if(bloop.RisingEdge())
    {
        //Trigger both envelopes!
        bloopVolGate = true;
        bloopFreqGate = true;
    }
    //If you RELEASE the bloop button...
    if(bloop.FallingEdge())
    {
        //End the sustain
        bloopVolGate = false;
        bloopFreqGate = false;
    }

    //If you press the bass button trigger its envelope
    if(bass.RisingEdge())
    {
        bassEnv.Trig();
    }

    //If you press the snare button trigger its envelope
    if(snare.RisingEdge())
    {
        snareEnv.Trig();
    }

    //If press the tiss button trigger its envelope
    if(tiss.RisingEdge())
    {
        tissEnv.Trigger();
    }

    //Prepare the audio block
    for(size_t i = 0; i < size; i += 2)
    {
        //TODO: Make some external functions to clean up this loop
        //TODO: Only call .Process() when an envelope is triggered
        //TODO: Processing by frame instead of by sample?

        //Get the next volume samples
        tss_env_out = tissEnv.Process();
        clk_env_out = clickVolEnv.Process();
        blp_env_out = bloopVolEnv.Process(bloopVolGate);

        freqMod = (1 + hardware.adc.GetFloat(0));
        freq_set = base_f * freqMod;
        //Modify the click pitch envelope according to pot input
        clickPitchEnv.SetMax(freq_set);
        clickPitchEnv.SetMin((freq_set) - 550);

        //Apply the pitch envelope to the click
        clickOsc.SetFreq(clickPitchEnv.Process());
        //Set the click volume to the envelope's output
        clickOsc.SetAmp(clk_env_out);
        //Process the next oscillator sample
        clickOsc_out = clickOsc.Process();
        (clickOsc_out != 0) ? nOsc++ : 1 ;

        //Set the click volume to the envelope's output
        bloopOsc.SetAmp(blp_env_out);
        //Apply the pitch envelope to the bloop
        bloopOsc.SetFreq(freq_set * bloopPitchEnv.Process(bloopFreqGate));
        //Process the next oscillator sample
        bloopOsc_out = bloopOsc.Process();
        (bloopOsc_out != 0) ? nOsc++ : 1 ;
        //TODO: Bloop osc envelope lasts way too long... detect when freq < audible

        //Process the next bass sample
        bassOsc_out = bassEnv.Process();
        (bassOsc_out != 0) ? nOsc++ : 1 ;
/*
        //Process the next snare sample
        snareOsc_out = snareEnv.Process();
        (snareOsc_out != 0) ? nOsc++ : 1 ;
*/
        //Get the next tiss sample
        noise_out = noise.Process();
        //Set the sample to the correct volume
        noise_out *= tss_env_out;
        
        //TODO: add proper compression instead of this nonsense

        if (nOsc == 0){ osc_out = 0; }
        else { osc_out = (clickOsc_out + bassOsc_out + snareOsc_out + bloopOsc_out) / nOsc; }
        nOsc = 0;
        
        //Mix the two signals
        sig = .3 * noise_out + .7 * osc_out;

        //Set the left and right outputs to the mixed signals
        out[i]     = sig;
        out[i + 1] = sig;
    }
}

int main(void)
{
    // Configure and Initialize the Daisy Seed
    // These are separate to allow reconfiguration of any of the internal
    // components before initialization.
    hardware.Configure();
    hardware.Init();
    hardware.SetAudioBlockSize(4);
    float samplerate = hardware.AudioSampleRate();

    tick.Init(1.0f, samplerate);
    
    //Initialize the adc
    adcConfig.InitSingle(hardware.GetPin(15));
    hardware.adc.Init(&adcConfig, 1);
    //Start reading values
    hardware.adc.Start();

    //Initialize oscillator for click drum
    clickOsc.Init(samplerate);
    clickOsc.SetWaveform(Oscillator::WAVE_TRI);
    clickOsc.SetAmp(1);

    //Initialize oscillator for bloop drum
    bloopOsc.Init(samplerate);
    bloopOsc.SetWaveform(Oscillator::WAVE_TRI);
    bloopOsc.SetFreq(base_f);
    bloopOsc.SetAmp(1);

    //Initialize noise
    noise.Init();

    //Initialize the bass and snare drums
    bassEnv.Init(samplerate);
    snareEnv.Init(samplerate);

    //Initialize envelopes, this one's for the tiss amplitude
    tissEnv.Init(samplerate);
    tissEnv.SetTime(ADENV_SEG_ATTACK, .01);
    tissEnv.SetTime(ADENV_SEG_DECAY, .2);
    tissEnv.SetMax(1);
    tissEnv.SetMin(0);

    //This envelope will control the click oscillator's pitch
    //Note that this envelope is much faster than the volume
    clickPitchEnv.Init(samplerate);
    clickPitchEnv.SetTime(ADENV_SEG_ATTACK, .01);
    clickPitchEnv.SetTime(ADENV_SEG_DECAY, .05);
    clickPitchEnv.SetMax(base_f);
    clickPitchEnv.SetMin(50);

    //This one will control the click's volume
    clickVolEnv.Init(samplerate);
    clickVolEnv.SetTime(ADENV_SEG_ATTACK, .01);
    clickVolEnv.SetTime(ADENV_SEG_DECAY, 1);
    clickVolEnv.SetMax(1);
    clickVolEnv.SetMin(0);

    //Bloop oscillator pitch envelope
    bloopPitchEnv.Init(samplerate);
    bloopPitchEnv.SetTime(ADSR_SEG_ATTACK, .01);
    bloopPitchEnv.SetTime(ADSR_SEG_DECAY, .05);
    bloopPitchEnv.SetTime(ADSR_SEG_RELEASE, .5);
    bloopPitchEnv.SetSustainLevel(0.5);

    //Bloop oscillator volume envelope
    bloopVolEnv.Init(samplerate);
    bloopVolEnv.SetTime(ADSR_SEG_ATTACK, .01);
    bloopVolEnv.SetTime(ADSR_SEG_DECAY, 1);
    bloopVolEnv.SetTime(ADSR_SEG_RELEASE, 1);
    bloopVolEnv.SetSustainLevel(0.7);
    
    //Initialize buttons on pins 24 - 28
    //The callback rate is samplerate / blocksize (48)
    bloop.Init(hardware.GetPin(24), samplerate / 48.f);
    snare.Init(hardware.GetPin(25), samplerate / 48.f);
    bass.Init(hardware.GetPin(26), samplerate / 48.f);
    tiss.Init(hardware.GetPin(27), samplerate / 48.f);
    click.Init(hardware.GetPin(28), samplerate / 48.f);

    //Start calling the callback function
    hardware.StartAudio(AudioCallback);

    // Loop forever
    for(;;) {}
}