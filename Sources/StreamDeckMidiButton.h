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
//#include "RtMidi.h"
#include <rtmidi17.hpp>
#include "base64.h"
#include "timer.h" //written by Martin Vorbrodt - https://vorbrodt.blog/
#include <mutex>
#include <fstream>
#include <CoreServices/CoreServices.h>

#define DEFAULT_PORT_NAME "Streamdeck MIDI"

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
const int NOTE_ON = 143;
const int NOTE_OFF = 127;
const int CC = 175;
const int PC = 191;
}

//enum Direction for MIDI In/Out
enum class Direction
{
    OUT,
    IN,
};

class StreamDeckMidiButton : public ESDBasePlugin
{
public:
    //constructor
	StreamDeckMidiButton();
    //destructor
	virtual ~StreamDeckMidiButton();
	
    //handle button presses
	void KeyDownForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
	void KeyUpForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
    
    //handle appearance of the action
	void WillAppearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
	void WillDisappearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
	
    //handle connection & disconnection of the Stream Deck
	void DeviceDidConnect(const std::string& inDeviceID, const json &inDeviceInfo) override;
	void DeviceDidDisconnect(const std::string& inDeviceID) override;
	
    //handle sending & receiving information from the plugin and PI
	void SendToPlugin(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
    void DidReceiveGlobalSettings(const json& inPayload) override;
    void DidReceiveSettings(const std::string& inAction, const std::string& inContext, const json& inPayload, const std::string& inDeviceID) override;

    //set an action icon
    void SetActionIcon(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID);
        
private:
    //deal with initialising MIDI
    bool InitialiseMidi(Direction direction);
    
    //get a map of the available input & output MIDI ports
    std::map<std::string, std::string> GetMidiPortList(Direction direction);
    
    //does what it says on the tin
    void InitialSetup();
    
    //MIDI input callback function
    void HandleMidiInput(const rtmidi::message &message);
    
    //functions to update the fade sets and send midi messages
    void UpdateTimer();
    void UpdateFade();
    
    //send a midi message
    void SendMidiMessage(const std::vector<unsigned char>& midiMessage);
    
    void ChangeButtonState(const std::string& inContext);
    void StoreButtonSettings(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID);
    
    //reading & writing of files
    std::vector<unsigned char> ReadFile(const char* filename);
    bool WriteFile(const char* filename, std::string string);
        
    std::string GetPngAsString(const char* filename);
    
    //Global Settings
    struct GlobalSettings
    {
        std::string portName = DEFAULT_PORT_NAME;
        int selectedOutPortIndex = 0;
        int selectedInPortIndex = 0;
        std::string selectedOutPortName;
        std::string selectedInPortName;
        bool useVirtualPort = false;
        bool printDebug = false;//set to false so we don't create lots of unnecessary log files
        int sampleInterval = 5;
    };
    
    //Button Settings
    struct ButtonSettings
    {
        //common settings
        std::string inAction;
        std::string inContext;
        
        int statusByte = 144; //channel and message - defaults to NOTE ON channel 1
        int dataByte1 = 0; //note on, note off, CC, PC, MMC
        int dataByte2 = 0; //velocity or CC value
        int dataByte2Alt = 0; //CC alternate value
        int dataByte5 = 0; //MMC message
        
        //noteOn&Off settings
        int noteOffMode = 0;
        bool toggleNoteOnOff = true;
        
        //state settings for multiaction buttons
        bool state = false;
        
        //mode for CC buttons
        int ccMode = 0;
        
        //initial fade values - used to calculate the fade sets when necessary
        bool toggleFade = false;
        float fadeTime = 0;
        float fadeCurve = 0;
        
        //midiMMC settings
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
            void FadeButtonPressed();
            int setSize = 0;
            int fromValue = 0;
            int toValue = 0;
            std::vector<float> inSet;
            std::vector<float> outSet;

        private:
            int currentIndex = 0;
            int inPrevValue = 0;
            int outPrevValue = 0;
            float intervalSize = 0;
            bool reverseFade = false;
    };
    
    //global MIDI message
    std::vector<unsigned char> midiMessage;

    //PortSettings stored globally
    GlobalSettings *mGlobalSettings;
    
    // MUTEXES //
    //button mutexes
    std::mutex mVisibleContextsMutex;
	std::set<std::string> mVisibleContexts;

    //mutex to lock the midi message so we don't overwrite things
    std::mutex midiMessageMutex;
        
    //mutex to lock the midi input so we don't crash
    std::mutex midiUpdateMutex;
    
    //Rtmidi17
    rtmidi::midi_out *midiOut = nullptr;
    rtmidi::midi_in *midiIn = nullptr;

    //Timer
    Timer *eTimer;
    
    //maps of the various structs
    std::map<std::string, ButtonSettings> storedButtonSettings;
    std::map<int, std::string> storedStatusBytes;
    std::map<std::string, FadeSet> storedFadeSettings;
    
    //initial setup flag - doesn't work properly yet
    std::once_flag initialSetup;
};

