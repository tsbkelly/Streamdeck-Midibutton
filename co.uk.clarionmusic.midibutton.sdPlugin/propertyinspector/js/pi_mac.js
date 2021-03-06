/** Step 1: Subscribe to the 'connected' event
 * and call your own initialization method.
 * The connected - event is emitted, when StreamDeck
 * has established a connection.
 * The 'connected' event carries a JSON object containing
 * necessary information about the connection and the
 * inital data.
 */

var uuid,
    actionInfo,
    inputDevices,
    outputDevices,
    settings,
    ctx;
    
const NOTE_ON = 143;
const NOTE_OFF = 127;
const CC = 175;
const PC = 191;


$SD.on("connected", (jsonObj) => connected(jsonObj));
$SD.on("didReceiveGlobalSettings", (jsonObj) => didReceiveGlobalSettings(jsonObj));
$SD.on('sendToPropertyInspector', (jsonObj) => receivedDataFromPlugin(jsonObj));

function connected(jsonObj)
{
    uuid = jsonObj.uuid;
    actionInfo = jsonObj.actionInfo.action;
    ctx = jsonObj.actionInfo.context;
    settings = jsonObj.actionInfo.payload.settings;

    //create and set the HTML of the property inspector here
    if (actionInfo == "uk.co.clarionmusic.midibutton.noteon")
    {
        console.log(settings);
        document.getElementById("messageSettingsDiv").insertAdjacentHTML('beforeend', `
            <div type="select" class="sdpi-item" id="midiChannelDiv">
                <div class="sdpi-item-label">Midi Channel</div>
                <select class="sdpi-item-value" id="midiChannel" onchange="saveSettings()">
                    <option value="1">1</option>
                    <option value="2">2</option>
                    <option value="3">3</option>
                    <option value="4">4</option>
                    <option value="5">5</option>
                    <option value="6">6</option>
                    <option value="7">7</option>
                    <option value="8">8</option>
                    <option value="9">9</option>
                    <option value="10">10</option>
                    <option value="11">11</option>
                    <option value="12">12</option>
                    <option value="13">13</option>
                    <option value="14">14</option>
                    <option value="15">15</option>
                    <option value="16">16</option>
                </select>
            </div>
            <div class="sdpi-item" id="midiNoteDiv">
                <div class="sdpi-item-label">MIDI Note</div>
                    <input type="number" min="0" max="127" class="sdpi-item-value" id="midiNote" value="" onchange="saveSettings()">
            </div>
            <div class="sdpi-item" id="midiVelocityDiv">
                <div class="sdpi-item-label">MIDI Velocity</div>
                    <input type="number" min="0" max="127" class="sdpi-item-value" id="midiVelocity" value="" onchange="saveSettings()">
            </div>
            <div type="select" class="sdpi-item" id="noteOffModeDiv">
                <div class="sdpi-item-label">Note Off Behaviour</div>
                <select class="sdpi-item-value" id="noteOffMode" onchange="saveSettings()">
                    <option value="0">No Note Off</option>
                    <option value="1">On Push</option>
                    <option value="2">On Release</option>
                </select>
            </div>
            `);
        
        document.getElementById("midiChannel").value = (settings.statusByte - NOTE_ON) || 1;
        document.getElementById("midiNote").value = settings.dataByte1 || 0;
        document.getElementById("midiVelocity").value = settings.dataByte2 || 1;
        document.getElementById("noteOffMode").value = settings.noteOffMode || 0;
        
        /*document.getElementById("midiChannel").value = settings.midiChannel || 1;
        document.getElementById("midiNote").value = settings.midiNote || 0;
        document.getElementById("midiVelocity").value = settings.midiVelocity || 1;
        document.getElementById("noteOffMode").value = settings.noteOffMode || 0;*/
    }
    else if (actionInfo == "uk.co.clarionmusic.midibutton.noteontoggle")
    {
        document.getElementById("messageSettingsDiv").insertAdjacentHTML('beforeend', `
            <div type="select" class="sdpi-item" id="midiChannelDiv">
                <div class="sdpi-item-label">Midi Channel</div>
                <select class="sdpi-item-value" id="midiChannel" onchange="saveSettings()">
                    <option value="1">1</option>
                    <option value="2">2</option>
                    <option value="3">3</option>
                    <option value="4">4</option>
                    <option value="5">5</option>
                    <option value="6">6</option>
                    <option value="7">7</option>
                    <option value="8">8</option>
                    <option value="9">9</option>
                    <option value="10">10</option>
                    <option value="11">11</option>
                    <option value="12">12</option>
                    <option value="13">13</option>
                    <option value="14">14</option>
                    <option value="15">15</option>
                    <option value="16">16</option>
                </select>
            </div>
            <div class="sdpi-item" id="midiNoteDiv">
                <div class="sdpi-item-label">MIDI Note</div>
                    <input type="number" min="0" max="127" class="sdpi-item-value" id="midiNote" value="" onchange="saveSettings()">
            </div>
            <div class="sdpi-item" id="midiVelocityDiv">
                <div class="sdpi-item-label">MIDI Velocity</div>
                    <input type="number" min="0" max="127" class="sdpi-item-value" id="midiVelocity" value="" onchange="saveSettings()">
            </div>
            `);
            
        document.getElementById("midiChannel").value = (settings.statusByte - NOTE_ON) || 1;
        document.getElementById("midiNote").value = settings.dataByte1 || 0;
        document.getElementById("midiVelocity").value = settings.dataByte2 || 1;

        /*document.getElementById("midiChannel").value = settings.midiChannel || 1;
        document.getElementById("midiNote").value = settings.midiNote || 0;
        document.getElementById("midiVelocity").value = settings.midiVelocity || 1;*/
    }
    else if (actionInfo == "uk.co.clarionmusic.midibutton.cc")
    {
        document.getElementById("messageSettingsDiv").innerHTML = `
        <div type="group" class="sdpi-item">
            <div type="select" class="sdpi-item" id="midiChannelDiv">
                <div class="sdpi-item-label">Midi Channel</div>
                <select class="sdpi-item-value" id="midiChannel" onchange="saveSettings()">
                    <option value="1">1</option>
                    <option value="2">2</option>
                    <option value="3">3</option>
                    <option value="4">4</option>
                    <option value="5">5</option>
                    <option value="6">6</option>
                    <option value="7">7</option>
                    <option value="8">8</option>
                    <option value="9">9</option>
                    <option value="10">10</option>
                    <option value="11">11</option>
                    <option value="12">12</option>
                    <option value="13">13</option>
                    <option value="14">14</option>
                    <option value="15">15</option>
                    <option value="16">16</option>
                </select>
            </div>
            <div type="select" class="sdpi-item" id="midiChannelDiv">
                <div class="sdpi-item-label">Midi Control Change</div>
                <select class="sdpi-item-value" id="midiCC" onchange="saveSettings()">
                    <option value="0">0 Bank Select (MSB)</option>
                    <option value="1">1 Modulation Wheel</option>
                    <option value="2">2 Breath controller</option>
                    <option value="3">3 Undefined</option>
                    <option value="4">4 Foot Pedal (MSB)</option>
                    <option value="5">5 Portamento Time (MSB)</option>
                    <option value="6">6 Data Entry (MSB) - cc100=0 & cc101=0 is pitch bend range</option>
                    <option value="7">7 Volume (MSB)</option>
                    <option value="8">8 Balance (MSB)</option>
                    <option value="9">9 Undefined</option>
                    <option value="10">10 Pan position (MSB)</option>
                    <option value="11">11 Expression (MSB)</option>
                    <option value="12">12 Effect Control 1 (MSB)</option>
                    <option value="13">13 Effect Control 2 (MSB)</option>
                    <option value="14">14 Undefined</option>
                    <option value="15">15 Undefined</option>
                    <option value="16">16 Ribbon Controller or General Purpose Slider 1</option>
                    <option value="17">17 Knob 1 or General Purpose Slider 2</option>
                    <option value="18">18 General Purpose Slider 3</option>
                    <option value="19">19 Knob 2 General Purpose Slider 4</option>
                    <option value="20">20 Knob 3 or Undefined</option>
                    <option value="21">21 Knob 4 or Undefined</option>
                    <option value="22">22 Undefined</option>
                    <option value="23">23 Undefined</option>
                    <option value="24">24 Undefined</option>
                    <option value="25">25 Undefined</option>
                    <option value="26">26 Undefined</option>
                    <option value="27">27 Undefined</option>
                    <option value="28">28 Undefined</option>
                    <option value="29">29 Undefined</option>
                    <option value="30">30 Undefined</option>
                    <option value="31">31 Undefined</option>
                    <option value="32">32 Bank Select (LSB)</option>
                    <option value="33">33 Modulation Wheel (LSB)</option>
                    <option value="34">34 Breath controller (LSB)</option>
                    <option value="35">35 Undefined</option>
                    <option value="36">36 Foot Pedal (LSB)</option>
                    <option value="37">37 Portamento Time (LSB)</option>
                    <option value="38">38 Data Entry (LSB)</option>
                    <option value="39">39 Volume (LSB)</option>
                    <option value="40">40 Balance (LSB)</option>
                    <option value="41">41 Undefined</option>
                    <option value="42">42 Pan position (LSB)</option>
                    <option value="43">43 Expression (LSB)</option>
                    <option value="44">44 Effect Control 1 (LSB)</option>
                    <option value="45">45 Effect Control 2 (LSB)</option>
                    <option value="46">46 Undefined</option>
                    <option value="47">47 Undefined</option>
                    <option value="48">48 Undefined</option>
                    <option value="49">49 Undefined</option>
                    <option value="50">50 Undefined</option>
                    <option value="51">51 Undefined</option>
                    <option value="52">52 Undefined</option>
                    <option value="53">53 Undefined</option>
                    <option value="54">54 Undefined</option>
                    <option value="55">55 Undefined</option>
                    <option value="56">56 Undefined</option>
                    <option value="57">57 Undefined</option>
                    <option value="58">58 Undefined</option>
                    <option value="59">59 Undefined</option>
                    <option value="60">60 Undefined</option>
                    <option value="61">61 Undefined</option>
                    <option value="62">62 Undefined</option>
                    <option value="63">63 Undefined</option>
                    <option value="64">64 Hold/Sustain Pedal (on/off)</option>
                    <option value="65">65 Portamento (on/off)</option>
                    <option value="66">66 Sustenuto Pedal (on/off)</option>
                    <option value="67">67 Soft Pedal (on/off)</option>
                    <option value="68">68 Legato Pedal (on/off)</option>
                    <option value="69">69 Hold 2 Pedal (on/off)</option>
                    <option value="70">70 Sound Variation</option>
                    <option value="71">71 Resonance (Timbre)</option>
                    <option value="72">72 Sound Release Time</option>
                    <option value="73">73 Sound Attack Time</option>
                    <option value="74">74 Frequency Cutoff</option>
                    <option value="75">75 Sound Control 6</option>
                    <option value="76">76 Sound Control 7</option>
                    <option value="77">77 Sound Control 8</option>
                    <option value="78">78 Sound Control 9</option>
                    <option value="79">79 Sound Control 10</option>
                    <option value="80">80 Decay or General Purpose Button 1 (on/off)</option>
                    <option value="81">81 Hi Pass Filter Frequency or General Purpose Button 2 (on/off)</option>
                    <option value="82">82 General Purpose Button 3 (on/off)</option>
                    <option value="83">83 General Purpose Button 4 (on/off)</option>
                    <option value="84">84 Undefined</option>
                    <option value="85">85 Undefined</option>
                    <option value="86">86 Undefined</option>
                    <option value="87">87 Undefined</option>
                    <option value="88">88 Undefined</option>
                    <option value="89">89 Undefined</option>
                    <option value="90">90 Undefined</option>
                    <option value="91">91 Reverb Level</option>
                    <option value="92">92 Tremolo Level</option>
                    <option value="93">93 Chorus Level</option>
                    <option value="94">94 Celeste Level or Detune</option>
                    <option value="95">95 Phaser Level</option>
                    <option value="96">96 Data Button increment</option>
                    <option value="97">97 Data Button decrement</option>
                    <option value="98">98 Non-registered Parameter (LSB)</option>
                    <option value="99">99 Non-registered Parameter (MSB)</option>
                    <option value="100">100 Registered Parameter (LSB)</option>
                    <option value="101">101 Registered Parameter (MSB)</option>
                    <option value="102">102 Undefined</option>
                    <option value="103">103 Undefined</option>
                    <option value="104">104 Undefined</option>
                    <option value="105">105 Undefined</option>
                    <option value="106">106 Undefined</option>
                    <option value="107">107 Undefined</option>
                    <option value="108">108 Undefined</option>
                    <option value="109">109 Undefined</option>
                    <option value="110">110 Undefined</option>
                    <option value="111">111 Undefined</option>
                    <option value="112">112 Undefined</option>
                    <option value="113">113 Undefined</option>
                    <option value="114">114 Undefined</option>
                    <option value="115">115 Undefined</option>
                    <option value="116">116 Undefined</option>
                    <option value="117">117 Undefined</option>
                    <option value="118">118 Undefined</option>
                    <option value="119">119 Undefined</option>
                    <option value="120">All Sound Off</option>
                    <option value="121">All Controllers Off</option>
                    <option value="122">Local Keyboard (on/off)</option>
                    <option value="123">All Notes Off</option>
                    <option value="124">Omni Mode Off</option>
                    <option value="125">Omni Mode On</option>
                    <option value="126">Mono Operation</option>
                    <option value="127">Poly Operation</option>
                </select>
            </div>
            <div type="select" class="sdpi-item" id="ccModeDiv">
                <div class="sdpi-item-label">Mode</div>
                <select class="sdpi-item-value" id="ccMode" onchange="saveSettings()">
                    <option value="0">Single Value</option>
                    <option value="1">Momentary Value</option>
                    <option value="2">Momentary with Fade In</option>
                    <option value="3">Momentary with Fade Out</option>
                </select>
            </div>
            <div class="sdpi-item" id="midiValueDiv">
                <div class="sdpi-item-label">Main CC Value</div>
                    <input type="number" min="0" max="127" class="sdpi-item-value" id="midiValue" value="" onchange="saveSettings()">
            </div>
            <div class="sdpi-item" id="midiValueSecDiv">
                <div class="sdpi-item-label">Secondary CC Value</div>
                    <input type="number" min="0" max="127" class="sdpi-item-value" id="midiValueSec" value="" onchange="saveSettings()">
            </div>
            <div class="sdpi-item" id="fadeTimeDiv">
                <div class="sdpi-item-label">Fade Time (secs.)</div>
                <input type="number" step="0.1" min="0" max="360" class="sdpi-item-value" id="fadeTime" value="1" onchange="saveSettings()">
            </div>
            <div class="sdpi-item" id="fadeCurveDiv">
                <div class="sdpi-item-label">Fade Curve</div>
                <input type="number" step="0.1" min="-5" max="5" class="sdpi-item-value" id="fadeCurve" value="" onchange="saveSettings()">
            </div>
        </div>
        `;
        
        document.getElementById("midiChannel").value = (settings.statusByte - CC) || 1;
        document.getElementById("midiCC").value = settings.dataByte1 || 0;
        document.getElementById("midiValue").value = settings.dataByte2 || 0;
        document.getElementById("midiValueSec").value = settings.dataByte2Alt || 0;

        /*document.getElementById("midiChannel").value = settings.midiChannel || 1;
        document.getElementById("midiCC").value = settings.midiCC || 0;
        document.getElementById("midiValue").value = settings.midiValue || 0;
        document.getElementById("midiValueSec").value = settings.midiValueSec || 0;*/

        document.getElementById("ccMode").value = settings.ccMode || 0;
        document.getElementById("fadeTime").value = settings.fadeTime || 1;
        document.getElementById("fadeCurve").value = settings.fadeCurve || 0;
        
        switch (document.getElementById("ccMode").value)
        {
            case "0":
                document.getElementById("midiValueSecDiv").style.display = "none";
                document.getElementById("fadeTimeDiv").style.display = "none";
                document.getElementById("fadeCurveDiv").style.display = "none";
                break;
            case "1":
                document.getElementById("midiValueSecDiv").style.display = "";
                document.getElementById("fadeTimeDiv").style.display = "none";
                document.getElementById("fadeCurveDiv").style.display = "none";
                break;
            case "2": case "3":
                document.getElementById("midiValueSecDiv").style.display = "";
                document.getElementById("fadeTimeDiv").style.display = "";
                document.getElementById("fadeCurveDiv").style.display = "";
                break;
            default:
                document.getElementById("midiValueSecDiv").style.display = "none";
                document.getElementById("fadeTimeDiv").style.display = "none";
                document.getElementById("fadeCurveDiv").style.display = "none";
                break;
        }    }
    else if (actionInfo == "uk.co.clarionmusic.midibutton.cctoggle")
    {
        document.getElementById("messageSettingsDiv").insertAdjacentHTML('beforeend', `
        <div type="group" class="sdpi-item">
            <div type="select" class="sdpi-item" id="midiChannelDiv">
                <div class="sdpi-item-label">Midi Channel</div>
                <select class="sdpi-item-value" id="midiChannel" onchange="saveSettings()">
                    <option value="1">1</option>
                    <option value="2">2</option>
                    <option value="3">3</option>
                    <option value="4">4</option>
                    <option value="5">5</option>
                    <option value="6">6</option>
                    <option value="7">7</option>
                    <option value="8">8</option>
                    <option value="9">9</option>
                    <option value="10">10</option>
                    <option value="11">11</option>
                    <option value="12">12</option>
                    <option value="13">13</option>
                    <option value="14">14</option>
                    <option value="15">15</option>
                    <option value="16">16</option>
                </select>
            </div>
            <div type="select" class="sdpi-item" id="midiChannelDiv">
                <div class="sdpi-item-label">Midi Control Change</div>
                <select class="sdpi-item-value" id="midiCC" onchange="saveSettings()">
                    <option value="0">0 Bank Select (MSB)</option>
                    <option value="1">1 Modulation Wheel</option>
                    <option value="2">2 Breath controller</option>
                    <option value="3">3 Undefined</option>
                    <option value="4">4 Foot Pedal (MSB)</option>
                    <option value="5">5 Portamento Time (MSB)</option>
                    <option value="6">6 Data Entry (MSB) - cc100=0 & cc101=0 is pitch bend range</option>
                    <option value="7">7 Volume (MSB)</option>
                    <option value="8">8 Balance (MSB)</option>
                    <option value="9">9 Undefined</option>
                    <option value="10">10 Pan position (MSB)</option>
                    <option value="11">11 Expression (MSB)</option>
                    <option value="12">12 Effect Control 1 (MSB)</option>
                    <option value="13">13 Effect Control 2 (MSB)</option>
                    <option value="14">14 Undefined</option>
                    <option value="15">15 Undefined</option>
                    <option value="16">16 Ribbon Controller or General Purpose Slider 1</option>
                    <option value="17">17 Knob 1 or General Purpose Slider 2</option>
                    <option value="18">18 General Purpose Slider 3</option>
                    <option value="19">19 Knob 2 General Purpose Slider 4</option>
                    <option value="20">20 Knob 3 or Undefined</option>
                    <option value="21">21 Knob 4 or Undefined</option>
                    <option value="22">22 Undefined</option>
                    <option value="23">23 Undefined</option>
                    <option value="24">24 Undefined</option>
                    <option value="25">25 Undefined</option>
                    <option value="26">26 Undefined</option>
                    <option value="27">27 Undefined</option>
                    <option value="28">28 Undefined</option>
                    <option value="29">29 Undefined</option>
                    <option value="30">30 Undefined</option>
                    <option value="31">31 Undefined</option>
                    <option value="32">32 Bank Select (LSB)</option>
                    <option value="33">33 Modulation Wheel (LSB)</option>
                    <option value="34">34 Breath controller (LSB)</option>
                    <option value="35">35 Undefined</option>
                    <option value="36">36 Foot Pedal (LSB)</option>
                    <option value="37">37 Portamento Time (LSB)</option>
                    <option value="38">38 Data Entry (LSB)</option>
                    <option value="39">39 Volume (LSB)</option>
                    <option value="40">40 Balance (LSB)</option>
                    <option value="41">41 Undefined</option>
                    <option value="42">42 Pan position (LSB)</option>
                    <option value="43">43 Expression (LSB)</option>
                    <option value="44">44 Effect Control 1 (LSB)</option>
                    <option value="45">45 Effect Control 2 (LSB)</option>
                    <option value="46">46 Undefined</option>
                    <option value="47">47 Undefined</option>
                    <option value="48">48 Undefined</option>
                    <option value="49">49 Undefined</option>
                    <option value="50">50 Undefined</option>
                    <option value="51">51 Undefined</option>
                    <option value="52">52 Undefined</option>
                    <option value="53">53 Undefined</option>
                    <option value="54">54 Undefined</option>
                    <option value="55">55 Undefined</option>
                    <option value="56">56 Undefined</option>
                    <option value="57">57 Undefined</option>
                    <option value="58">58 Undefined</option>
                    <option value="59">59 Undefined</option>
                    <option value="60">60 Undefined</option>
                    <option value="61">61 Undefined</option>
                    <option value="62">62 Undefined</option>
                    <option value="63">63 Undefined</option>
                    <option value="64">64 Hold/Sustain Pedal (on/off)</option>
                    <option value="65">65 Portamento (on/off)</option>
                    <option value="66">66 Sustenuto Pedal (on/off)</option>
                    <option value="67">67 Soft Pedal (on/off)</option>
                    <option value="68">68 Legato Pedal (on/off)</option>
                    <option value="69">69 Hold 2 Pedal (on/off)</option>
                    <option value="70">70 Sound Variation</option>
                    <option value="71">71 Resonance (Timbre)</option>
                    <option value="72">72 Sound Release Time</option>
                    <option value="73">73 Sound Attack Time</option>
                    <option value="74">74 Frequency Cutoff</option>
                    <option value="75">75 Sound Control 6</option>
                    <option value="76">76 Sound Control 7</option>
                    <option value="77">77 Sound Control 8</option>
                    <option value="78">78 Sound Control 9</option>
                    <option value="79">79 Sound Control 10</option>
                    <option value="80">80 Decay or General Purpose Button 1 (on/off)</option>
                    <option value="81">81 Hi Pass Filter Frequency or General Purpose Button 2 (on/off)</option>
                    <option value="82">82 General Purpose Button 3 (on/off)</option>
                    <option value="83">83 General Purpose Button 4 (on/off)</option>
                    <option value="84">84 Undefined</option>
                    <option value="85">85 Undefined</option>
                    <option value="86">86 Undefined</option>
                    <option value="87">87 Undefined</option>
                    <option value="88">88 Undefined</option>
                    <option value="89">89 Undefined</option>
                    <option value="90">90 Undefined</option>
                    <option value="91">91 Reverb Level</option>
                    <option value="92">92 Tremolo Level</option>
                    <option value="93">93 Chorus Level</option>
                    <option value="94">94 Celeste Level or Detune</option>
                    <option value="95">95 Phaser Level</option>
                    <option value="96">96 Data Button increment</option>
                    <option value="97">97 Data Button decrement</option>
                    <option value="98">98 Non-registered Parameter (LSB)</option>
                    <option value="99">99 Non-registered Parameter (MSB)</option>
                    <option value="100">100 Registered Parameter (LSB)</option>
                    <option value="101">101 Registered Parameter (MSB)</option>
                    <option value="102">102 Undefined</option>
                    <option value="103">103 Undefined</option>
                    <option value="104">104 Undefined</option>
                    <option value="105">105 Undefined</option>
                    <option value="106">106 Undefined</option>
                    <option value="107">107 Undefined</option>
                    <option value="108">108 Undefined</option>
                    <option value="109">109 Undefined</option>
                    <option value="110">110 Undefined</option>
                    <option value="111">111 Undefined</option>
                    <option value="112">112 Undefined</option>
                    <option value="113">113 Undefined</option>
                    <option value="114">114 Undefined</option>
                    <option value="115">115 Undefined</option>
                    <option value="116">116 Undefined</option>
                    <option value="117">117 Undefined</option>
                    <option value="118">118 Undefined</option>
                    <option value="119">119 Undefined</option>
                    <option value="120">All Sound Off</option>
                    <option value="121">All Controllers Off</option>
                    <option value="122">Local Keyboard (on/off)</option>
                    <option value="123">All Notes Off</option>
                    <option value="124">Omni Mode Off</option>
                    <option value="125">Omni Mode On</option>
                    <option value="126">Mono Operation</option>
                    <option value="127">Poly Operation</option>
                </select>
            </div>
            <div class="sdpi-item" id="midiValueDiv">
                <div class="sdpi-item-label">Main CC Value</div>
                    <input type="number" min="0" max="127" class="sdpi-item-value" id="midiValue" value="" onchange="saveSettings()">
            </div>
            <div class="sdpi-item" id="midiValueSecDiv">
                <div class="sdpi-item-label">Secondary CC Value</div>
                    <input type="number" min="0" max="127" class="sdpi-item-value" id="midiValueSec" value="" onchange="saveSettings()">
            </div>
            <div type="checkbox" class="sdpi-item" id="toggleFadeDiv">
                <div class="sdpi-item-label">Fade Between States</div>
                <div class="sdpi-item-value min100">
                    <div class="sdpi-item-child">
                        <input class="sdpi-item-value" id="toggleFade" type="checkbox" onchange="saveSettings()">
                        <label for="toggleFade"><span></span></label>
                    </div>
                </div>
            </div>
            <div class="sdpi-item" id="fadeTimeDiv" style="display:none">
                <div class="sdpi-item-label">Fade Time (secs.)</div>
                <input type="number" step="0.1" min="0" max="360" class="sdpi-item-value" id="fadeTime" value="1" onchange="saveSettings()">
            </div>
            <div class="sdpi-item" id="fadeCurveDiv" style="display:none">
                <div class="sdpi-item-label">Fade Curve</div>
                <input type="number" step="0.1" min="-5" max="5" class="sdpi-item-value" id="fadeCurve" value="" onchange="saveSettings()">
            </div>
        </div>
        `);
        document.getElementById("midiChannel").value = (settings.statusByte - CC) || 1;
        document.getElementById("midiCC").value = settings.dataByte1 || 0;
        document.getElementById("midiValue").value = settings.dataByte2 || 0;
        document.getElementById("midiValueSec").value = settings.dataByte2Alt || 0;

        /*document.getElementById("midiChannel").value = settings.midiChannel || 1;
        document.getElementById("midiCC").value = settings.midiCC || 0;
        document.getElementById("midiValue").value = settings.midiValue || 0;
        document.getElementById("midiValueSec").value = settings.midiValueSec || 0;*/

        document.getElementById("toggleFade").checked = settings.toggleFade || 0;
        document.getElementById("fadeTime").value = settings.fadeTime || 1;
        document.getElementById("fadeCurve").value = settings.fadeCurve || 0;
        document.getElementById("fadeTimeDiv").style.display = document.getElementById("toggleFade").checked ? "" : "none";
        document.getElementById("fadeCurveDiv").style.display = document.getElementById("toggleFade").checked ? "" : "none";
    }
    else if (actionInfo == "uk.co.clarionmusic.midibutton.programchange")
    {
        document.getElementById("messageSettingsDiv").innerHTML = `
            <div type="select" class="sdpi-item" id="midiChannelDiv">
                <div class="sdpi-item-label">Midi Channel</div>
                <select class="sdpi-item-value" id="midiChannel" onchange="saveSettings()">
                    <option value="1">1</option>
                    <option value="2">2</option>
                    <option value="3">3</option>
                    <option value="4">4</option>
                    <option value="5">5</option>
                    <option value="6">6</option>
                    <option value="7">7</option>
                    <option value="8">8</option>
                    <option value="9">9</option>
                    <option value="10">10</option>
                    <option value="11">11</option>
                    <option value="12">12</option>
                    <option value="13">13</option>
                    <option value="14">14</option>
                    <option value="15">15</option>
                    <option value="16">16</option>
                </select>
            </div>
            <div class="sdpi-item">
                <div class="sdpi-item-label">MIDI Program Change Number</div>
                    <input type="number" min="0" max="127" class="sdpi-item-value" id="midiProgramChange" value="1" onchange="saveSettings()">
            </div>
        `;
        document.getElementById("midiChannel").value = (settings.statusByte - PC) || 1;
        document.getElementById("midiProgramChange").value = settings.dataByte1 || 0;

        /*document.getElementById("midiChannel").value = settings.midiChannel || 1;
        document.getElementById("midiProgramChange").value = settings.midiProgramChange || 0;*/
    }
    else if (actionInfo == "uk.co.clarionmusic.midibutton.mmc")
    {
        document.getElementById("messageSettingsDiv").innerHTML = `
            <div type="select" class="sdpi-item" id="midiMMCDiv">
                <div class="sdpi-item-label">MMC Message</div>
                <select class="sdpi-item-value" id="midiMMC" onchange="saveSettings()">
                    <option value="1">Stop</option>
                    <option value="2">Play</option>
                    <option value="4">Fast Forward</option>
                    <option value="5">Rewind</option>
                    <option value="6">Record</option>
                    <option value="9">Pause</option>
                </select>
            </div>
        `;
        
        document.getElementById("midiMMC").value = settings.dataByte5 || 1;
        
        //document.getElementById("midiMMC").value = settings.midiMMC || 1;
    }
    document.getElementById("messageSettingsDiv").insertAdjacentHTML('beforeend', `
    <details>
        <summary>Setup</summary>
        <div type="checkbox" class="sdpi-item" id="options">
            <div class="sdpi-item-label">Options</div>
            <div class="sdpi-item-value min100">
                <div class="sdpi-item-child">
                    <input class="sdpi-item-value" id="printDebug" type="checkbox" onchange="saveGlobalSettings()">
                    <label for="printDebug"><span></span>Debug</label>
                </div>
                <div class="sdpi-item-child" id="virtualPortCheckBoxDiv">
                    <input class="sdpi-item-value" id="useVirtualPort" type="checkbox" onchange="saveGlobalSettings()">
                    <label for="useVirtualPort"><span></span>Use Virtual Port</label>
                </div>
            </div>
        </div>
        <div class="sdpi-item" id="virtualPortName">
            <div class="sdpi-item-label">Virtual Port Name</div>
            <input class="sdpi-item-value" id="portName" value="Streamdeck MIDI" onchange="saveGlobalSettings()">
        </div>
        <div type="select" class="sdpi-item" id="inPortList">
            <div class="sdpi-item-label">Input Port</div>
            <select class="sdpi-item-value select" id="inPortListSelector" onchange="saveGlobalSettings()">
            </select>
        </div>
        <div type="select" class="sdpi-item" id="outPortList">
            <div class="sdpi-item-label">Output Port</div>
            <select class="sdpi-item-value select" id="outPortListSelector" onchange="saveGlobalSettings()">
            </select>
        </div>
    </details>`);
    $SD.api.getGlobalSettings(uuid);
}

function didReceiveGlobalSettings(jsonObj)
{
    settings = jsonObj.payload.settings;
    document.getElementById("printDebug").checked = settings.printDebug;
    document.getElementById("virtualPortCheckBoxDiv").style.display = "none";
    if (navigator.appVersion.indexOf("Mac") != -1)
    {
        document.getElementById("virtualPortCheckBoxDiv").style.display = "";
        document.getElementById("useVirtualPort").checked = settings.useVirtualPort;
    }
    document.getElementById("portName").value = settings.portName;
    if (!settings.useVirtualPort) $SD.api.sendToPlugin(uuid, actionInfo, {event: "getMidiPorts"});
    document.getElementById("virtualPortName").style.display = document.getElementById("useVirtualPort").checked ? "" : "none";
    document.getElementById("outPortList").style.display = document.getElementById("useVirtualPort").checked ? "none" : "";
    document.getElementById("inPortList").style.display = document.getElementById("useVirtualPort").checked ? "none" : "";
    document.getElementById("virtualPortName").style.display = document.getElementById("useVirtualPort").checked ? "" : "none";
}
        
function receivedDataFromPlugin(jsonObj)
{
    const payload = jsonObj["payload"];
    if (payload["event"] == "midiOutPorts")
    {
        const outPortSelector = document.getElementById("outPortListSelector");
        while (outPortSelector.firstChild)
        {
            outPortSelector.removeChild(outPortSelector.firstChild);
        }
        var count = 0;
        for (i in payload["midiOutPortList"])
        {
            const outPortOption = document.createElement("option");
            outPortOption.text = i;
            //console.log("Midi OUT port with name " + i + " has index: " + payload["midiOutPortList"][i]);
            outPortOption.value = payload["midiOutPortList"][i];
            outPortSelector.appendChild(outPortOption);
            count++;
        }
    }
    else if (payload["event"] == "midiOutPortSelected")
    {
        document.getElementById("outPortListSelector").value = payload.midiOutPortSelected;
    }
    else if (payload["event"] == "midiInPorts")
    {
        const inPortSelector = document.getElementById("inPortListSelector");
        while (inPortSelector.firstChild)
        {
            inPortSelector.removeChild(inPortSelector.firstChild);
        }

        var count = 0;
        for (i in payload["midiInPortList"])
        {
            const inPortOption = document.createElement("option");
            inPortOption.text = i;
            //console.log("Midi IN port with name " + i + " has index: " + payload["midiInPortList"][i]);
            inPortOption.value = payload["midiInPortList"][i];
            inPortSelector.appendChild(inPortOption);
            count++;
        }
    }
    else if (payload["event"] == "midiInPortSelected")
    {
        document.getElementById("inPortListSelector").value = payload.midiInPortSelected;
    }
    else return;
}
                                                                     
function saveSettings()
{
    //clear the current settings
    for (var key in settings) delete settings[key];
    //save settings here
    if (actionInfo == "uk.co.clarionmusic.midibutton.noteon")
    {
        settings.statusByte = (parseInt(document.getElementById("midiChannel").value) + NOTE_ON);
        settings.dataByte1 = parseInt(document.getElementById("midiNote").value);
        settings.dataByte2 = parseInt(document.getElementById("midiVelocity").value);

        /*settings.midiChannel = parseInt(document.getElementById("midiChannel").value);
        settings.midiNote = parseInt(document.getElementById("midiNote").value);
        settings.midiVelocity = parseInt(document.getElementById("midiVelocity").value);*/
        
        settings.noteOffMode = parseInt(document.getElementById("noteOffMode").value);
    }
    else if (actionInfo == "uk.co.clarionmusic.midibutton.noteontoggle")
    {
        settings.statusByte = (parseInt(document.getElementById("midiChannel").value) + NOTE_ON);
        settings.dataByte1 = parseInt(document.getElementById("midiNote").value);
        settings.dataByte2 = parseInt(document.getElementById("midiVelocity").value);

        /*settings.midiChannel = parseInt(document.getElementById("midiChannel").value);
        settings.midiNote = parseInt(document.getElementById("midiNote").value);
        settings.midiVelocity = parseInt(document.getElementById("midiVelocity").value);*/
    }
    else if (actionInfo == "uk.co.clarionmusic.midibutton.programchange")
    {
        settings.statusByte = (parseInt(document.getElementById("midiChannel").value) + PC);
        settings.dataByte1 = parseInt(document.getElementById("midiProgramChange").value);

        /*settings.midiChannel = parseInt(document.getElementById("midiChannel").value);
        settings.midiProgramChange = parseInt(document.getElementById("midiProgramChange").value);*/
    }
    else if (actionInfo == "uk.co.clarionmusic.midibutton.cc")
    {
        settings.statusByte = (parseInt(document.getElementById("midiChannel").value) + CC);
        settings.dataByte1 = parseInt(document.getElementById("midiCC").value);
        settings.dataByte2 = parseInt(document.getElementById("midiValue").value);
        settings.dataByte2Alt = parseInt(document.getElementById("midiValueSec").value);

        /*settings.midiChannel = parseInt(document.getElementById("midiChannel").value);
        settings.midiCC = parseInt(document.getElementById("midiCC").value);
        settings.midiValue = parseInt(document.getElementById("midiValue").value);
        settings.midiValueSec = parseInt(document.getElementById("midiValueSec").value);*/
        
        settings.ccMode = parseInt(document.getElementById("ccMode").value);
        settings.fadeTime = parseFloat(document.getElementById("fadeTime").value);
        settings.fadeCurve = parseFloat(document.getElementById("fadeCurve").value);
        switch (document.getElementById("ccMode").value)
        {
            case "0":
                document.getElementById("midiValueSecDiv").style.display = "none";
                document.getElementById("fadeTimeDiv").style.display = "none";
                document.getElementById("fadeCurveDiv").style.display = "none";
                break;
            case "1":
                document.getElementById("midiValueSecDiv").style.display = "";
                document.getElementById("fadeTimeDiv").style.display = "none";
                document.getElementById("fadeCurveDiv").style.display = "none";
                break;
            case "2": case "3":
                document.getElementById("midiValueSecDiv").style.display = "";
                document.getElementById("fadeTimeDiv").style.display = "";
                document.getElementById("fadeCurveDiv").style.display = "";
                break;
            default:
                document.getElementById("midiValueSecDiv").style.display = "none";
                document.getElementById("fadeTimeDiv").style.display = "none";
                document.getElementById("fadeCurveDiv").style.display = "none";
                break;
        }
    }
    else if (actionInfo == "uk.co.clarionmusic.midibutton.cctoggle")
    {
        settings.statusByte = (parseInt(document.getElementById("midiChannel").value) + CC);
        settings.dataByte1 = parseInt(document.getElementById("midiCC").value);
        settings.dataByte2 = parseInt(document.getElementById("midiValue").value);
        settings.dataByte2Alt = parseInt(document.getElementById("midiValueSec").value);

        /*settings.midiChannel = parseInt(document.getElementById("midiChannel").value);
        settings.midiCC = parseInt(document.getElementById("midiCC").value);
        settings.midiValue = parseInt(document.getElementById("midiValue").value);
        settings.midiValueSec = parseInt(document.getElementById("midiValueSec").value);*/

        settings.fadeTime = parseFloat(document.getElementById("fadeTime").value);
        settings.fadeCurve = parseFloat(document.getElementById("fadeCurve").value);
        settings.toggleFade = document.getElementById("toggleFade").checked;
        document.getElementById("fadeTimeDiv").style.display = document.getElementById("toggleFade").checked ? "" : "none";
        document.getElementById("fadeCurveDiv").style.display = document.getElementById("toggleFade").checked ? "" : "none";
    }
    else if (actionInfo == "uk.co.clarionmusic.midibutton.mmc")
    {
        settings.dataByte5 = parseInt(document.getElementById("midiMMC").value);
        //set the icon of the action to the correct image
        $SD.api.sendToPlugin(uuid, actionInfo, {midiMMC: settings.midiMMC});
    }
    $SD.api.setSettings(uuid, settings);
}

function saveGlobalSettings()
{
    //clear the current settings
    for (var key in settings) delete settings[key];
                                                                     
    //save settings here
    settings.printDebug = document.getElementById("printDebug").checked;
    settings.useVirtualPort = document.getElementById("useVirtualPort").checked;
    settings.portName = document.getElementById("portName").value;
    settings.selectedOutPortIndex = +document.getElementById("outPortListSelector").value;
    settings.selectedInPortIndex = +document.getElementById("inPortListSelector").value;
    
    try
    {
        settings.selectedOutPortName = document.getElementById("outPortListSelector").options[document.getElementById("outPortListSelector").selectedIndex].text;
        settings.selectedInPortName = document.getElementById("inPortListSelector").options[document.getElementById("inPortListSelector").selectedIndex].text;
    }
    catch (error)
    {
    //nothing to do here
    }
    
    $SD.api.setGlobalSettings(uuid, settings);
    if (!settings.useVirtualPort) $SD.api.sendToPlugin(uuid, actionInfo, {event: "getMidiPorts"});
    document.getElementById("virtualPortName").style.display = document.getElementById("useVirtualPort").checked ? "" : "none";
    document.getElementById("outPortList").style.display = document.getElementById("useVirtualPort").checked ? "none" : "";
    document.getElementById("inPortList").style.display = document.getElementById("useVirtualPort").checked ? "none" : "";
}

function getMidiNote(note)
{
    if ((typeof note === 'number' || typeof note === 'string') && note > 0 && note < 128) return +note;
    var p = Array.isArray(note) ? note : parse(note);
    if (!p || p.length < 2) return null;
    return p[0] * 7 + p[1] * 12 + 12;
}
