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
#include <mutex>

#define DEFAULT_PORT_NAME "Streamdeck MIDI"

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
    void DidReceiveSettings(const std::string& inAction, const std::string& inContext, const json& inPayload, const std::string& inDeviceID);
    
    void SetActionIcon(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID);

private:
    
    void SendNoteOn(const int midiChannel, const int midiNote, const int midiVelocity, const bool sendNoteOff);
    void SendCC(const int midiChannel, const int midiCC, const int midiValue);
    void SendMMC(const int sendMMC);
    
    struct PortSettings {
      std::string portName = DEFAULT_PORT_NAME;

//      bool isValid() const;
//      json toJSON() const;
//      static PortSettings fromJSON(const json&);
    };
    //PortSettings stored globally
    PortSettings mPortSettings;
    
    std::mutex mVisibleContextsMutex;
	std::set<std::string> mVisibleContexts;
    
    RtMidiOut *midiOut;
};
