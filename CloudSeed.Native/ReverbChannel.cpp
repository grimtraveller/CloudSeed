
#include <cmath>
#include "ReverbChannel.h"
#include "AudioLib\ShaRandom.h"
#include "Utils.h"

namespace CloudSeed
{
	ReverbChannel::ReverbChannel(int bufferSize, int samplerate)
		: preDelay(bufferSize, 10000)
		, multitap(bufferSize)
		, highPass(samplerate)
		, lowPass(samplerate)
		, diffuser(bufferSize, samplerate)
	{
		for (int i = 0; i < TotalLineCount; i++)
			lines.push_back(new DelayLine(bufferSize, samplerate));
		
		this->bufferSize = bufferSize;

		for (auto value = 0; value < (int)Parameter::Count; value++)
			this->parameters[static_cast<Parameter>(value)] = 0.0;

		lineCount = 8;
		perLineGain = GetPerLineGain();

		highPass.SetCutoffHz(20);
		lowPass.SetCutoffHz(20000);

		tempBuffer = new double[bufferSize];
		lineOutBuffer = new double[bufferSize];
		outBuffer = new double[bufferSize];
		delayLineSeeds = AudioLib::ShaRandom::Generate(12345, lines.size() * 3);

		this->samplerate = samplerate;
	}

	ReverbChannel::~ReverbChannel()
	{
		for (auto line : lines)
			delete line;

		delete tempBuffer;
		delete lineOutBuffer;
		delete outBuffer;
	}

	int ReverbChannel::GetSamplerate()
	{
		return samplerate;
	}

	void ReverbChannel::SetSamplerate(int samplerate)
	{
		this->samplerate = samplerate;
		highPass.SetSamplerate(samplerate);
		lowPass.SetSamplerate(samplerate);

		for (size_t i = 0; i < lines.size(); i++)
		{
			lines[i]->SetSamplerate(samplerate);
		}

		auto update = [&](Parameter p) { SetParameter(p, parameters[p]); };
		update(Parameter::PreDelay);
		update(Parameter::TapLength);
		update(Parameter::DiffusionDelay);
		update(Parameter::LineDelay);
		update(Parameter::LateDiffusionDelay);
		update(Parameter::EarlyDiffusionModRate);
		update(Parameter::LineModRate);
		update(Parameter::LateDiffusionModRate);
		update(Parameter::LineModAmount);
		UpdateLines();
	}

	double* ReverbChannel::GetOutput()
	{
		return outBuffer;
	}

	double* ReverbChannel::GetLineOutput()
	{
		return lineOutBuffer;
	}

	void ReverbChannel::SetParameter(Parameter para, double value)
	{
		parameters[para] = value;

		switch (para)
		{
		case Parameter::PreDelay:
			preDelay.SampleDelay = (int)Ms2Samples(value);
			break;
		case Parameter::HighPass:
			highPass.SetCutoffHz(value);
			break;
		case Parameter::LowPass:
			lowPass.SetCutoffHz(value);
			break;

		case Parameter::TapCount:
			multitap.SetTapCount((int)value);
			break;
		case Parameter::TapLength:
			multitap.SetTapLength((int)Ms2Samples(value));
			break;
		case Parameter::TapGain:
			multitap.SetTapGain(value);
			break;
		case Parameter::TapDecay:
			multitap.SetTapDecay(value);
			break;

		case Parameter::DiffusionEnabled:
		{
			auto newVal = value >= 0.5;
			if (newVal != diffuserEnabled)
				diffuser.ClearBuffers();
			diffuserEnabled = newVal;
			break;
		}
		case Parameter::DiffusionStages:
			diffuser.Stages = (int)value;
			break;
		case Parameter::DiffusionDelay:
			diffuser.SetDelay((int)Ms2Samples(value));
			break;
		case Parameter::DiffusionFeedback:
			diffuser.SetFeedback(value);
			break;

		case Parameter::LineCount:
			lineCount = (int)value;
			perLineGain = GetPerLineGain();
			break;
		case Parameter::LineDelay:
			UpdateLines();
			break;
		case Parameter::LineFeedback:
			UpdateLines();
			break;

		case Parameter::LateDiffusionEnabled:
			for (auto line : lines)
			{
				auto newVal = value >= 0.5;
				if (newVal != line->DiffuserEnabled)
					line->ClearDiffuserBuffer();
				line->DiffuserEnabled = newVal;
			}
			break;
		case Parameter::LateDiffusionStages:
			for(auto line : lines)
				line->SetDiffuserStages((int)value);
			break;
		case Parameter::LateDiffusionDelay:
			for (auto line : lines)
				line->SetDiffuserDelay((int)Ms2Samples(value));
			break;
		case Parameter::LateDiffusionFeedback:
			for (auto line : lines)
				line->SetDiffuserFeedback(value);
			break;

		case Parameter::PostLowShelfGain:
			for (auto line : lines)
				line->SetLowShelfGain(value);
			break;
		case Parameter::PostLowShelfFrequency:
			for (auto line : lines)
				line->SetLowShelfFrequency(value);
			break;
		case Parameter::PostHighShelfGain:
			for (auto line : lines)
				line->SetHighShelfGain(value);
			break;
		case Parameter::PostHighShelfFrequency:
			for (auto line : lines)
				line->SetHighShelfFrequency(value);
			break;
		case Parameter::PostCutoffFrequency:
			for (auto line : lines)
				line->SetCutoffFrequency(value);
			break;

		case Parameter::EarlyDiffusionModAmount:
			diffuser.SetModulationEnabled(value > 0.0);
			diffuser.SetModAmount(Ms2Samples(value));
			break;
		case Parameter::EarlyDiffusionModRate:
			diffuser.SetModRate(value);
			break;
		case Parameter::LineModAmount:
			UpdateLines();
			break;
		case Parameter::LineModRate:
			UpdateLines();
			break;
		case Parameter::LateDiffusionModAmount:
			UpdateLines();
			break;
		case Parameter::LateDiffusionModRate:
			UpdateLines();
			break;

		case Parameter::TapSeed:
			multitap.SetSeeds(AudioLib::ShaRandom::Generate((int)value, 100));
			break;
		case Parameter::DiffusionSeed:
			diffuser.SetSeeds(AudioLib::ShaRandom::Generate((int)value, AllpassDiffuser::MaxStageCount * 3));
			break;
		case Parameter::CombSeed:
			delayLineSeeds = AudioLib::ShaRandom::Generate((int)value, lines.size() * 3);
			UpdateLines();
			break;
		case Parameter::PostDiffusionSeed:
			for (size_t i = 0; i < lines.size(); i++)
				lines[i]->SetDiffuserSeeds(AudioLib::ShaRandom::Generate(((long long)value) * (i + 1), AllpassDiffuser::MaxStageCount * 3));
			break;

		case Parameter::DryOut:
			dryOut = value;
			break;
		case Parameter::PredelayOut:
			predelayOut = value;
			break;
		case Parameter::EarlyOut:
			earlyOut = value;
			break;
		case Parameter::MainOut:
			lineOut = value;
			break;

		case Parameter::HiPassEnabled:
			highPassEnabled = value >= 0.5;
			break;
		case Parameter::LowPassEnabled:
			lowPassEnabled = value >= 0.5;
			break;
		case Parameter::LowShelfEnabled:
			for (auto line : lines)
				line->LowShelfEnabled = value >= 0.5;
			break;
		case Parameter::HighShelfEnabled:
			for (auto line : lines)
				line->HighShelfEnabled = value >= 0.5;
			break;
		case Parameter::CutoffEnabled:
			for (auto line : lines)
				line->CutoffEnabled = value >= 0.5;
			break;
		case Parameter::LateStageTap:
			for (auto line : lines)
				line->LateStageTap = value >= 0.5;
			break;

		case Parameter::Interpolation:
			diffuser.SetInterpolationEnabled(value >= 0.5);
			for (auto line : lines)
				line->SetInterpolationEnabled(value >= 0.5);
			break;
		case Parameter::SampleResolution:
			for (auto line : lines)
				line->SampleResolution = value;
			break;
		case Parameter::Undersampling:
			for (auto line : lines)
				line->Undersampling = value;
			break;
		}
	}

	void ReverbChannel::Process(double* input, int sampleCount)
	{
		int len = sampleCount;
		auto predelayOutput = preDelay.GetOutput();
		auto lowPassInput = highPassEnabled ? tempBuffer : input;

		if (highPassEnabled)
			highPass.Process(input, tempBuffer, len);
		if (lowPassEnabled)
			lowPass.Process(lowPassInput, tempBuffer, len);
		if (!lowPassEnabled && !highPassEnabled)
			Utils::Copy(input, tempBuffer, len);

		// completely zero if no input present
		// Previously, the very small values were causing some really strange CPU spikes
		for (int i = 0; i < len; i++)
		{
			auto n = tempBuffer[i];
			if (n * n < 0.000000001)
				tempBuffer[i] = 0;
		}

		preDelay.Process(tempBuffer, len);
		multitap.Process(preDelay.GetOutput(), len);

		auto earlyOutStage = diffuserEnabled ? diffuser.GetOutput() : multitap.GetOutput();

		if (diffuserEnabled)
		{
			diffuser.Process(multitap.GetOutput(), len);
			Utils::Copy(diffuser.GetOutput(), tempBuffer, len);
		}
		else
		{
			Utils::Copy(multitap.GetOutput(), tempBuffer, len);
		}

		// mix in the feedback from the other channel
		//for (int i = 0; i < len; i++)
		//	tempBuffer[i] += crossMix[i];
		
		for (int i = 0; i < lineCount; i++)
			lines[i]->Process(tempBuffer, len);

		for (int i = 0; i < lineCount; i++)
		{
			auto buf = lines[i]->GetOutput();

			if (i == 0)
			{
				for (int j = 0; j < len; j++)
					tempBuffer[j] = buf[j];
			}
			else
			{
				for (int j = 0; j < len; j++)
					tempBuffer[j] += buf[j];
			}
		}

		Utils::Gain(tempBuffer, perLineGain, len);
		Utils::Copy(tempBuffer, lineOutBuffer, len);

		for (int i = 0; i < len; i++)
		{
			outBuffer[i] =
				dryOut           * input[i] +
				predelayOut      * predelayOutput[i] +
				earlyOut         * earlyOutStage[i] +
				lineOut          * tempBuffer[i];
		}
	}

	void ReverbChannel::ClearBuffers()
	{
		for (int i = 0; i < bufferSize; i++)
		{
			tempBuffer[i] = 0.0;
			lineOutBuffer[i] = 0.0;
			outBuffer[i] = 0.0;
		}

		lowPass.Output = 0;
		highPass.Output = 0;

		preDelay.ClearBuffers();
		multitap.ClearBuffers();
		diffuser.ClearBuffers();
		for (auto line : lines)
			line->ClearBuffers();
	}
	
	double ReverbChannel::GetPerLineGain()
	{
		return 1 / std::sqrt(lineCount);
	}
	
	void ReverbChannel::UpdateLines()
	{
		auto lineDelay = (int)Ms2Samples(parameters[Parameter::LineDelay]);
		auto lineFeedback = parameters[Parameter::LineFeedback];

		auto lineModAmount = Ms2Samples(parameters[Parameter::LineModAmount]);
		auto lineModRate = parameters[Parameter::LineModRate];
		
		auto lateDiffusionModAmount = Ms2Samples(parameters[Parameter::LateDiffusionModAmount]);
		auto lateDiffusionModRate = parameters[Parameter::LateDiffusionModRate];
		
		if (lineDelay < 50) lineDelay = 50;

		int count = lines.size();
		for (int i = 0; i < count; i++)
		{
			auto delay = (0.1 + 0.9 * delayLineSeeds[i]) * lineDelay;
			auto ratio = delay / lineDelay;
			auto adjustedFeedback = std::pow(lineFeedback, ratio);

			auto modAmount = lineModAmount * (0.8 + 0.2 * delayLineSeeds[i + count]);
			auto modRate = lineModRate * (0.8 + 0.2 * delayLineSeeds[i + 2 * count]) / samplerate;

			lines[i]->SetDelay((int)delay);
			lines[i]->SetFeedback(adjustedFeedback);
			lines[i]->SetLineModAmount(modAmount);
			lines[i]->SetLineModRate(modRate);
			lines[i]->SetDiffuserModAmount(lateDiffusionModAmount);
			lines[i]->SetDiffuserModRate(lateDiffusionModRate);
		}
	}

	double ReverbChannel::Ms2Samples(double value)
	{
		return value / 1000.0 * samplerate;
	}

}
