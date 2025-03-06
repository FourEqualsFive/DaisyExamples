#include "daisy_seed.h"
#include "daisysp.h"

// Use the daisy namespace to prevent having to type
// daisy:: before all libdaisy functions
using namespace daisy;
using namespace daisysp;

// Declare a DaisySeed object called hardware
DaisySeed hardware;

Oscillator clickOsc, bassOsc, snareOsc;
WhiteNoise tissNoise;

AdEnv clickVolEnv, clickPitchEnv, tissEnv, popEnv;
AnalogBassDrum bassVolEnv, bassPitchEnv;
AnalogSnareDrum snareVolEnv, snarePitchEnv;

Switch click, tiss, bass, snare;

void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    int nOsc = 0;
    float osc_out, clickOsc_out;
    float noise_out, tiss_env_out, click_env_out, sig;
    float bassOsc_out, snareOsc_out;
    float bass_env_out, snare_env_out;
    
    //Get rid of any bouncing
    tiss.Debounce();
    click.Debounce();
    bass.Debounce();
    snare.Debounce();

    //If you press the click button...
    if(click.RisingEdge())
    {
        //Trigger both envelopes!
        clickVolEnv.Trigger();
        clickPitchEnv.Trigger();
    }

    //If you press the bass button...
    if(bass.RisingEdge())
    {
        //Trigger both envelopes!
        bassVolEnv.Trig();
        bassPitchEnv.Trig();
    }

    //If you press the snare button...
    if(snare.RisingEdge())
    {
        //Trigger both envelopes!
        snareVolEnv.Trig();
        snarePitchEnv.Trig();
    }

    //If press the tiss button trigger its envelope
    if(tiss.RisingEdge())
    {
        tissEnv.Trigger();
    }

    //Prepare the audio block
    for(size_t i = 0; i < size; i += 2)
    {
        //Get the next volume samples
        tiss_env_out = tissEnv.Process();
        click_env_out = clickVolEnv.Process();
        bass_env_out = bassVolEnv.Process();
        snare_env_out = snareVolEnv.Process();

        //Apply the pitch envelope to the click
        clickOsc.SetFreq(clickPitchEnv.Process());
        //Set the click volume to the envelope's output
        clickOsc.SetAmp(click_env_out);
        //Process the next oscillator sample
        clickOsc_out = clickOsc.Process();
        (clickOsc_out != 0) ? nOsc++ : 1 ;

        //Apply the pitch envelope to the bass
        bassOsc.SetFreq(bassPitchEnv.Process());
        //Set the click volume to the envelope's output
        bassOsc.SetAmp(bass_env_out);
        //Process the next oscillator sample
        bassOsc_out = bassOsc.Process();
        (bassOsc_out != 0) ? nOsc++ : 1 ;

        //Apply the pitch envelope to the snare
        snareOsc.SetFreq(snarePitchEnv.Process());
        //Set the click volume to the envelope's output
        snareOsc.SetAmp(snare_env_out);
        //Process the next oscillator sample
        snareOsc_out = snareOsc.Process();
        (snareOsc_out != 0) ? nOsc++ : 1 ;

        //Get the next tiss sample
        noise_out = tissNoise.Process();
        //Set the sample to the correct volume
        noise_out *= tiss_env_out;

        if (nOsc == 0){ osc_out = 0; }
        else { osc_out = clickOsc_out; }
        //else { osc_out = (clickOsc_out + bassOsc_out + snareOsc_out) / nOsc; }

        //Mix the two signals at half volume
        sig = .5 * noise_out + .5 * osc_out;

        //Set the left and right outputs to the mixed signals
        out[i]     = sig;
        out[i + 1] = sig;

        nOsc = 0;
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

    //Initialize oscillator for clickdrum
    clickOsc.Init(samplerate);
    clickOsc.SetWaveform(Oscillator::WAVE_TRI);
    clickOsc.SetAmp(1);

    //Initialize oscillator for bassdrum
    bassOsc.Init(samplerate);
    bassOsc.SetWaveform(Oscillator::WAVE_TRI);
    bassOsc.SetAmp(1);

    //Initialize oscillator for snaredrum
    snareOsc.Init(samplerate);
    snareOsc.SetWaveform(Oscillator::WAVE_TRI);
    snareOsc.SetAmp(1);

    //Initialize noise
    tissNoise.Init();

    //Initialize tiss amplitude envelope
    tissEnv.Init(samplerate);
    tissEnv.SetTime(ADENV_SEG_ATTACK, .01);
    tissEnv.SetTime(ADENV_SEG_DECAY, .2);
    tissEnv.SetMax(1);
    tissEnv.SetMin(0);

    //Initialize click pitch envelope
    //Note that this envelope is much faster than the volume
    clickPitchEnv.Init(samplerate);
    clickPitchEnv.SetTime(ADENV_SEG_ATTACK, .01);
    clickPitchEnv.SetTime(ADENV_SEG_DECAY, .05);
    clickPitchEnv.SetMax(700);
    clickPitchEnv.SetMin(50);

    //Initialize click volume envelope
    clickVolEnv.Init(samplerate);
    clickVolEnv.SetTime(ADENV_SEG_ATTACK, .01);
    clickVolEnv.SetTime(ADENV_SEG_DECAY, 1);
    clickVolEnv.SetMax(0.75);
    clickVolEnv.SetMin(0);

    //Initialize bass pitch envelope
    //Note that this envelope is much faster than the volume
    bassPitchEnv.Init(samplerate);

    //Initialize bass volume envelope
    bassVolEnv.Init(samplerate);

    //Initialize snare pitch envelope
    //Note that this envelope is much faster than the volume
    snarePitchEnv.Init(samplerate);

    //Initialize snare volume envelope
    snareVolEnv.Init(samplerate);

    //Initialize the click and tiss buttons on pins 27 and 28
    //The callback rate is samplerate / blocksize (48)
    tiss.Init(hardware.GetPin(27), samplerate / 48.f);
    click.Init(hardware.GetPin(28), samplerate / 48.f);
    bass.Init(hardware.GetPin(26), samplerate / 48.f);
    snare.Init(hardware.GetPin(25), samplerate / 48.f);

    //Start calling the callback function
    hardware.StartAudio(AudioCallback);

    // Loop forever
    for(;;) {}
}