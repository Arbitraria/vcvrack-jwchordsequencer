#include "plugin.hpp"
#include <memory>

// ============================
// Constants and Enums
// ============================

enum ParamIds {
	LENGTH_PARAM,
	BEATS_PER_BAR_PARAM,
	BAR_SELECT_PARAM,
	ROOT_PARAM,
	CHORD_PARAM,
	NUM_PARAMS
};

enum InputIds {
	CLOCK_INPUT,
	RESET_INPUT,
	NUM_INPUTS
};

enum OutputIds {
	ROOT_OUTPUT,
	CHORD_OUTPUT,
	NUM_OUTPUTS
};

enum LightIds {
	BAR_LIGHT,
	NUM_LIGHTS
};

// JW Modules 17 scale names (abbreviated for display)
static const char* scaleNames[17] = {
	"Aeo", "Blu", "Chr", "DMin", "Dor",
	"HMin", "Ind", "Loc", "Lyd", "Maj",
	"MMin", "Min", "Mix", "NMin", "Pent",
	"Phr", "Tur"
};

static const char* noteNames[12] = {
	"C", "C#", "D", "D#", "E", "F",
	"F#", "G", "G#", "A", "A#", "B"
};

// ============================
// Data Structures
// ============================

struct BarData {
	int root = 0;   // 0-11 (C-B)
	int chord = 9;  // 0-16, default to Major (index 9)
};

// ============================
// Module
// ============================

struct BarChordSeq : rack::engine::Module {
	BarData bars[32];
	int currentBar = 0;
	int currentBeat = 0;

	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::PulseGenerator barLightPulse;

	BarChordSeq() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(LENGTH_PARAM, 1.f, 32.f, 32.f, "Sequence Length");
		configParam(BEATS_PER_BAR_PARAM, 1.f, 16.f, 4.f, "Beats per Bar");
		configParam(BAR_SELECT_PARAM, 0.f, 31.f, 0.f, "Bar Select");
		configParam(ROOT_PARAM, 0.f, 11.f, 0.f, "Root Note");
		configParam(CHORD_PARAM, 0.f, 16.f, 9.f, "Chord/Scale");

		// Make parameters snap to integer values
		paramQuantities[LENGTH_PARAM]->snapEnabled = true;
		paramQuantities[BEATS_PER_BAR_PARAM]->snapEnabled = true;
		paramQuantities[BAR_SELECT_PARAM]->snapEnabled = true;
		paramQuantities[ROOT_PARAM]->snapEnabled = true;
		paramQuantities[CHORD_PARAM]->snapEnabled = true;

		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");

		configOutput(ROOT_OUTPUT, "Root CV");
		configOutput(CHORD_OUTPUT, "Chord/Scale CV");

		// Initialize all bars to C Major
		onReset();
	}

	void onReset() override {
		for (int i = 0; i < 32; i++) {
			bars[i].root = 0;
			bars[i].chord = 9;  // Major
		}
		currentBar = 0;
		currentBeat = 0;
	}

	void process(const ProcessArgs& args) override {
		// Read parameters
		int seqLength = (int)params[LENGTH_PARAM].getValue();
		int beatsPerBar = (int)params[BEATS_PER_BAR_PARAM].getValue();
		int selectedBar = (int)params[BAR_SELECT_PARAM].getValue();
		int selectedRoot = (int)params[ROOT_PARAM].getValue();
		int selectedChord = (int)params[CHORD_PARAM].getValue();

		// Clamp values
		seqLength = clamp(seqLength, 1, 32);
		beatsPerBar = clamp(beatsPerBar, 1, 16);
		selectedBar = clamp(selectedBar, 0, 31);
		selectedRoot = clamp(selectedRoot, 0, 11);
		selectedChord = clamp(selectedChord, 0, 16);

		// Write current selection to selected bar
		bars[selectedBar].root = selectedRoot;
		bars[selectedBar].chord = selectedChord;

		// Handle reset
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
			currentBar = 0;
			currentBeat = 0;
			barLightPulse.trigger(0.1f);
		}

		// Handle clock
		if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
			currentBeat++;
			if (currentBeat >= beatsPerBar) {
				currentBeat = 0;
				currentBar++;
				if (currentBar >= seqLength) {
					currentBar = 0;
				}
				barLightPulse.trigger(0.1f);
			}
		}

		// Clamp current bar to valid range
		currentBar = clamp(currentBar, 0, seqLength - 1);

		// Output CVs for current bar
		float rootCV = bars[currentBar].root / 12.0f;  // 1V/oct: 1 semitone = 1/12V
		float chordCV = (float)bars[currentBar].chord; // 1V per index
		outputs[ROOT_OUTPUT].setVoltage(rootCV);
		outputs[CHORD_OUTPUT].setVoltage(chordCV);

		// Update bar indicator light
		lights[BAR_LIGHT].setBrightness(barLightPulse.process(args.sampleTime));
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		// Save all bars
		json_t* barsJ = json_array();
		for (int i = 0; i < 32; i++) {
			json_t* barJ = json_object();
			json_object_set_new(barJ, "root", json_integer(bars[i].root));
			json_object_set_new(barJ, "chord", json_integer(bars[i].chord));
			json_array_append_new(barsJ, barJ);
		}
		json_object_set_new(rootJ, "bars", barsJ);

		// Save playback position
		json_object_set_new(rootJ, "currentBar", json_integer(currentBar));
		json_object_set_new(rootJ, "currentBeat", json_integer(currentBeat));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// Load all bars
		json_t* barsJ = json_object_get(rootJ, "bars");
		if (barsJ) {
			for (int i = 0; i < 32; i++) {
				json_t* barJ = json_array_get(barsJ, i);
				if (barJ) {
					json_t* rootV = json_object_get(barJ, "root");
					json_t* chordV = json_object_get(barJ, "chord");
					if (rootV) bars[i].root = json_integer_value(rootV);
					if (chordV) bars[i].chord = json_integer_value(chordV);
				}
			}
		}

		// Load playback position
		json_t* currentBarJ = json_object_get(rootJ, "currentBar");
		json_t* currentBeatJ = json_object_get(rootJ, "currentBeat");
		if (currentBarJ) currentBar = json_integer_value(currentBarJ);
		if (currentBeatJ) currentBeat = json_integer_value(currentBeatJ);
	}
};

// ============================
// Widget
// ============================

// Helper function to create labels
ui::Label* createLabel(Vec pos, std::string text, float fontSize = 11.0f) {
	ui::Label* label = new ui::Label();
	label->box.pos = pos;
	label->text = text;
	label->fontSize = fontSize;
	label->color = nvgRGB(0x44, 0x44, 0x44);  // Dark gray
	return label;
}

// Custom widget to display currently playing chord
struct ChordDisplay : ui::Label {
	BarChordSeq* module;

	ChordDisplay() {
		fontSize = 11.0f;
		color = nvgRGB(0x44, 0x44, 0x44);  // Match other labels
	}

	void step() override {
		if (module) {
			// Show currently playing bar's chord
			int root = module->bars[module->currentBar].root;
			int chord = module->bars[module->currentBar].chord;
			text = std::string(noteNames[root]) + " " + scaleNames[chord];
		} else {
			// Browser preview mode
			text = "C Maj";
		}
		ui::Label::step();
	}
};

struct BarChordSeqWidget : rack::app::ModuleWidget {
	BarChordSeqWidget(BarChordSeq* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/BarChordSeq.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Title (centered at top)
		addChild(createLabel(mm2px(Vec(20.32, 7)), "Bar Chord", 12.0f));
		addChild(createLabel(mm2px(Vec(20.32, 11)), "Sequencer", 10.0f));

		// Parameter labels (above each knob)
		addChild(createLabel(mm2px(Vec(20.32, 14)), "LENGTH", 9.0f));
		addChild(createLabel(mm2px(Vec(20.32, 32)), "BEATS/BAR", 8.0f));
		addChild(createLabel(mm2px(Vec(20.32, 50)), "BAR", 9.0f));
		addChild(createLabel(mm2px(Vec(10.0, 77)), "ROOT", 8.0f));
		addChild(createLabel(mm2px(Vec(30.64, 77)), "CHORD", 8.0f));

		// Input labels (above each jack)
		addChild(createLabel(mm2px(Vec(10.0, 101)), "CLOCK", 8.0f));
		addChild(createLabel(mm2px(Vec(30.64, 101)), "RESET", 8.0f));

		// Output labels (above each jack)
		addChild(createLabel(mm2px(Vec(10.0, 113)), "ROOT", 8.0f));
		addChild(createLabel(mm2px(Vec(30.64, 113)), "CHORD", 8.0f));

		// Parameters
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(20.32, 18)), module, LENGTH_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(20.32, 36)), module, BEATS_PER_BAR_PARAM));
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(20.32, 54)), module, BAR_SELECT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.0, 84)), module, ROOT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(30.64, 84)), module, CHORD_PARAM));

		// Inputs
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.0, 108)), module, CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(30.64, 108)), module, RESET_INPUT));

		// Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.0, 120)), module, ROOT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(30.64, 120)), module, CHORD_OUTPUT));

		// LED
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(20.32, 70)), module, BAR_LIGHT));

		// Chord display (shows currently playing chord)
		ChordDisplay* chordDisplay = new ChordDisplay();
		chordDisplay->box.pos = mm2px(Vec(20.32 - 15, 73.5));  // Centered between LED and knobs
		chordDisplay->module = module;
		addChild(chordDisplay);
	}
};

// ============================
// Model Registration
// ============================

Model* modelBarChordSeq = createModel<BarChordSeq, BarChordSeqWidget>("BarChordSeq");
