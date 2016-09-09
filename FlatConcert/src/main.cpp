#include <iostream>
#include <string>
#include <algorithm>

#include <stdio.h>
#include <limits.h>

#include <SFML/Audio.hpp>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window/Mouse.hpp>

#include <glm/glm.hpp>
#include <glm/vec2.hpp>

#include "portaudiocpp/PortAudioCpp.hxx"

using glm::vec2;

class Entity
{
public:
	virtual void setPosition(vec2 p)
	{
		position = p;
	}

	inline vec2 getPosition() const
	{
		return position;
	}

	float distance(vec2 p) const
	{
		return glm::distance(position, p);
	}

	float distance(const Entity& other) const
	{
		return distance(other.getPosition());
	}

	float distance2(vec2 p) const
	{
		vec2 diff = position - p;
		return dot(diff,diff);
	}

	float distance2(const Entity& other) const
	{
		return distance2(other.getPosition());
	}
protected:
	vec2 position;
};

class AudioSource : public Entity
{
public:
	AudioSource()
	{
	}

	AudioSource(const sf::SoundBuffer& buffer)
	{
		if(buffer.getChannelCount() != 1)//Only assept mono
		{
			std::cout << "Error: audio have to be mono" << std::endl;
			return;
		}
		sampleRate = buffer.getSampleRate();
		sampleCount = buffer.getSampleCount();
		sampleBuffer = new float[sampleCount];
		for(int i=0;i<sampleCount;i++)
			sampleBuffer[i] = (double)buffer.getSamples()[i]/(double)SHRT_MAX;
	}

	virtual ~AudioSource()
	{
		delete [] sampleBuffer;
		sampleBuffer = nullptr;
		sampleRate = 0;
		sampleCount = 0;
	}

	inline float* getSamples()
	{
		return sampleBuffer;
	}

	size_t getSampleCount() const
	{
		return sampleCount;
	}

	int getSampleRate() const
	{
		return sampleRate;
	}
protected:
	float* sampleBuffer = nullptr;
	int sampleRate = 0;
	size_t sampleCount = 0;
};

class Ear  : public Entity
{
public:
protected:
};

class Audience : public Entity
{
public:
	Audience()
	{
		updateEars();
	}

	virtual ~Audience()
	{
		delete [] ears;
	}

	void updateEars()
	{
		if(ears == nullptr)
			ears = new Ear[2];
		ears[0].setPosition(getPosition()-vec2(distanceBtwEars/2,0));//L ear
		ears[1].setPosition(getPosition()+vec2(distanceBtwEars/2,0));//R ear
	}

	virtual void setPosition(vec2 p)
	{
		Entity::setPosition(p);
		updateEars();
	}

	Ear* getEars()
	{
		return ears;
	}
protected:
	float distanceBtwEars = 0.15;
	Ear* ears = nullptr;
};

class Synthesiser
{
public:
	void setAudience(Audience* ptr)
	{
		audience = ptr;
	}

	const Audience* getAudience() const
	{
		return audience;
	}

	void setAudioSource(AudioSource* audioSource)
	{
		source = audioSource;
	}

	const AudioSource* getAudioSource() const
	{
		return source;
	}

	int generate(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags)
	{
		static int currentFrame = 0;
		assert(outputBuffer != NULL);

		float **out = static_cast<float **>(outputBuffer);

		int maxSampleNum = source->getSampleCount();

		float dist2LEar = audience->getEars()[0].distance2(*source);
		float dist2REar = audience->getEars()[1].distance2(*source);
		int sampleRate = source->getSampleRate();
		float delayR = sampleRate*sqrtf(dist2REar)/speedOfsound;
		float delayL = sampleRate*sqrtf(dist2LEar)/speedOfsound;
		float foo;
		float interR = std::modf(delayR,&foo);
		float interL = std::modf(delayL,&foo);

		for (unsigned int i = 0; i < framesPerBuffer; ++i)
		{
			int sampleIndex = i+currentFrame;
			int indexR = sampleIndex - delayR;
			int indexL = sampleIndex - delayL;

			if(sampleIndex == 0)
			{
				std::cout << indexL << "\t" << indexR << std::endl;
				std::cout << dist2LEar << "\t" << dist2REar << std::endl;
			}

			float sampleR1 = (indexR > 0 && indexR < maxSampleNum) ? source->getSamples()[indexR] : 0;
			float sampleR2 = (indexR > 0 && indexR + 1 < maxSampleNum) ? source->getSamples()[indexR] : 0;
			float sampleL1 = (indexL > 0 && indexL < maxSampleNum) ? source->getSamples()[indexL] : 0;
			float sampleL2 = (indexL > 0 && indexL + 1 < maxSampleNum) ? source->getSamples()[indexL] : 0;

			float sampleR = sampleR1*interR + sampleR2*(1.f-interR);
			float sampleL = sampleL1*interL + sampleL2*(1.f-interL);

			float ampR = 1.f/dist2REar;
			float ampL = 1.f/dist2LEar;

			//clamp max amplitude, just so the audio won't be too loud
			ampR = std::min(ampR, 3.f);
			ampL = std::min(ampL, 3.f);

			out[1][i] = sampleR*ampR;
			out[0][i] = sampleL*ampL;
		}

		currentFrame = currentFrame + framesPerBuffer;

		return paContinue;
	}
protected:
	Audience* audience;
	AudioSource* source;
	float speedOfsound = 340.29;
};

class PlaybackSystem
{
public:
	PlaybackSystem()
	{
		const int SAMPLE_RATE = 44100;
		const int FRAMES_PER_BUFFER = 8;//low sample num is required for good doppler effect simulation;
		portaudio::System &sys = portaudio::System::instance();
		outParams = new portaudio::DirectionSpecificStreamParameters(sys.defaultOutputDevice()
			, 2, portaudio::FLOAT32, false
			, sys.defaultOutputDevice().defaultLowOutputLatency(), NULL);
		params = new portaudio::StreamParameters(portaudio::DirectionSpecificStreamParameters::null()
			, *outParams, SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff);
	}

	void setSynthesiser(Synthesiser& synthesiser)
	{
		if(stream != nullptr)
			delete stream;
		stream = new portaudio::MemFunCallbackStream<Synthesiser>(*params, synthesiser, &Synthesiser::generate);
	}

	void play()
	{
		stream->start();
	}

	virtual ~PlaybackSystem()
	{
		stream->stop();
		stream->close();
		portaudio::System::instance().terminate();

		delete stream;
		delete outParams;
		delete params;
	}
protected:
	portaudio::AutoSystem autoSys;
	portaudio::DirectionSpecificStreamParameters* outParams;
	portaudio::StreamParameters* params;
	Synthesiser* synthesiser;
	portaudio::MemFunCallbackStream<Synthesiser>* stream = nullptr;

};


int main(int argc, char const *argv[])
{
	std::string filename;
	if(argc >= 2)
		filename = argv[1];
	else
	{
		std::cout << "Error: audio file path expected" << std::endl;
		return 1;
	}

	sf::RenderWindow window;
	sf::Vector2u windowSize(1200,720);
	window.create(sf::VideoMode(windowSize.x,windowSize.y), "Flat Concert");

	sf::SoundBuffer buffer;
	if (!buffer.loadFromFile(filename))
		return -1; // error

	sf::Sound sound(buffer);

	AudioSource source(buffer);
	Audience audience;
	PlaybackSystem playback;
	Synthesiser synthesiser;


	source.setPosition(vec2(0,0));
	audience.setPosition(vec2(0,-1));
	synthesiser.setAudioSource(&source);
	synthesiser.setAudience(&audience);
	playback.setSynthesiser(synthesiser);

	playback.play();

	sf::Texture texture;
	sf::Vector2u textureSize;
 	texture.loadFromFile("speaker.png");
	textureSize = texture.getSize();
	sf::Sprite sprite(texture);
	sprite.setPosition(windowSize.x/2-textureSize.x/2,windowSize.y/2-textureSize.y/2);

	bool rightButtonDown = false;

	while (window.isOpen())
	{
		// check all the window's events that were triggered since the last iteration of the loop
		sf::Event event;
		while (window.pollEvent(event))
		{
			// "close requested" event: we close the window
			if (event.type == sf::Event::Closed)
				window.close();
			else if(event.type == sf::Event::MouseButtonPressed)
			{
				if (event.mouseButton.button == sf::Mouse::Right)
				{
					rightButtonDown = true;
				}
			}
			else if(event.type == sf::Event::MouseButtonReleased)
			{
				if (event.mouseButton.button == sf::Mouse::Right)
				{
					rightButtonDown = false;
				}
			}
			else if (event.type == sf::Event::MouseMoved)
			{
				if(rightButtonDown)
				{
					sf::Vector2i position = sf::Mouse::getPosition(window);
					vec2 windowPos(position.x-(int)window.getSize().x/2,-(position.y-(int)window.getSize().y/2));
					float widnowsHeight = window.getSize().y;
					vec2 pos = windowPos/350.f;
					std::cout << pos.x << "\t" << pos.y << std::endl;
					pos*=6.0;
					audience.setPosition(pos);
				}
			}
		}

		window.clear();

		window.draw(sprite);

		window.display();
	}
	return 0;
}
