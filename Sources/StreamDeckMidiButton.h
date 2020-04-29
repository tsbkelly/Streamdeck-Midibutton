//==============================================================================
/**
@file       StreamDeckMidiButton.cpp

@brief      MIDI plugin

@copyright  (c) 2020, Clarion Music Ltd
            This source code is licensed under the MIT-style license found in the LICENSE file.
            Parts of this plugin (c) 2019 Fred Emmott, (c) 2018 Corsair Memory, Inc
            RtMIDI library

**/
//==============================================================================

#include "Common/ESDBasePlugin.h"
#include "RtMidi.h"
#include "base64.h"
#include <mutex>
#include <fstream>
#include <CoreServices/CoreServices.h>
#include <sys/stat.h>

// namespace
namespace {
const char* SEND_NOTE_ON = "uk.co.clarionmusic.midibutton.noteon";
const char* SEND_NOTE_ON_TOGGLE = "uk.co.clarionmusic.midibutton.noteontoggle";
const char* SEND_CC = "uk.co.clarionmusic.midibutton.cc";
const char* SEND_CC_TOGGLE = "uk.co.clarionmusic.midibutton.cctoggle";
const char* SEND_MMC = "uk.co.clarionmusic.midibutton.mmc";
const char* SEND_PROGRAM_CHANGE = "uk.co.clarionmusic.midibutton.programchange";
const int PUSH_NOTE_OFF = 1;
const int RELEASE_NOTE_OFF = 2;
}

//enum Direction for MIDI In/Out
enum class Direction {
  OUT,
  IN,
};

#define DEFAULT_PORT_NAME "Streamdeck MIDI"

class CallBackTimer
{
public:
    CallBackTimer();
    ~CallBackTimer();

    void stop();
    void start(int interval, std::function<void(void)> func);
    bool is_running() const noexcept;

private:
    std::atomic<bool> _execute;
    std::thread _thd;
};

class StreamDeckMidiButton : public ESDBasePlugin
{
public:
	
	StreamDeckMidiButton();
	virtual ~StreamDeckMidiButton();
	
	void KeyDownForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
	void KeyUpForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
	
	void WillAppearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
	void WillDisappearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
	
	void DeviceDidConnect(const std::string& inDeviceID, const json &inDeviceInfo) override;
	void DeviceDidDisconnect(const std::string& inDeviceID) override;
	
	void SendToPlugin(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
    void DidReceiveGlobalSettings(const json& inPayload) override;
    void DidReceiveSettings(const std::string& inAction, const std::string& inContext, const json& inPayload, const std::string& inDeviceID) override;
    
    void SetActionIcon(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID);

private:
    bool InitialiseMidi(Direction direction);
    std::map<std::string, std::string> GetMidiPortList(Direction direction);
    
    void GetMidiInput();
    void UpdateTimer();
    
    void SendNoteOn(const int midiChannel, const int midiNote, const int midiVelocity, const int sendNoteOff = 0);
    void SendNoteOff(const int midiChannel, const int midiNote, const int midiVelocity);
    void SendProgramChange(const int midiChannel, const int midiProgramChange);
    void SendCC(const int midiChannel, const int midiCC, const int midiValue);
    void SendMMC(const int sendMMC);
    
    std::vector<unsigned char> ReadFile(const char* filename);
    
    void StoreButtonSettings(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID);
    
    void MidiInputCallback(double deltatime, std::vector< unsigned char > *message, void *userData);
    
    std::string GetPngAsString(const char* filename);
    
    struct GlobalSettings {
        std::string portName = DEFAULT_PORT_NAME;
        int selectedOutPortIndex = 0;
        int selectedInPortIndex = 0;
        bool useVirtualPort = false;
        bool printDebug = true;
        bool initialSetup = false;
        int sampleInterval = 1;
    };
    
    struct ButtonSettings {
        
        //noteOn&Off settings
        int midiChannel = 1;
        int midiNote = 0;
        int midiVelocity = 1;
        int noteOffMode = 0;
        bool toggleNoteOnOff = true;
        
        //programChange settings
        int midiProgramChange = 0;
        
        //midiCC settings
        int midiCC = 0;
        int midiValue = 0;
        int midiValueSec = 0;
        
        //state settings for multiaction buttons
        bool state = false;
        
        //mode for CC buttons
        int ccMode = 0;
        
        //initial fade values - used to calculate the fade sets when necessary
        bool toggleFade = false;
        float fadeTime = 0;
        float fadeCurve = 0;
        
        //midiMMC settings
        int midiMMC = 0;
        bool midiMMCIsActive = false;//use for MIDI input triggerring MMC state change
    };
    
    struct FadeSet {
        public:
            bool fadeActive = false; //is the fade active
            bool fadeFinished = false; //is the fade finished - if so, send a green tick to the SD
            int currentValue = 0;
            Direction currentDirection;
            FadeSet ();
            FadeSet (const int fromValue, const int toValue, const float fadeTime, const float fadeCurve, const int sampleInterval);
            bool UpdateFade();
            void ReverseFade();
        private:
            std::vector<float> inSet;
            std::vector<float> outSet;
            int setSize = 0;
            int fromValue = 0;
            int toValue = 0;
            int currentIndex = 0;
            int inPrevValue = 0;
            int outPrevValue = 0;
            float intervalSize = 0;
            bool reverseFade = false;
    };
    
    //PortSettings stored globally
    GlobalSettings *mGlobalSettings;
    
    std::mutex mVisibleContextsMutex;
	std::set<std::string> mVisibleContexts;
    
    RtMidiOut *midiOut = nullptr;
    RtMidiIn *midiIn = nullptr;
    
    CallBackTimer *mTimer;
    
    std::map<std::string, ButtonSettings> storedButtonSettings;
    std::map<std::string, FadeSet> storedFadeSettings;
    
    std::once_flag initialSetup;
};


