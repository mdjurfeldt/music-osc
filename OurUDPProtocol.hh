class OurUDPProtocol {

public:
  
  static constexpr double TIMESTEP = 0.010;  // 10 ms
  static constexpr unsigned int KEYBOARDSIZE = 88;

  enum {SUSTAINCOMMAND = 0,
	PITCHBENDDOWNCOMMAND,
	MODULATECOMMAND,
	PITCHBENDUPCOMMAND,
	VOLUMECOMMAND,
	COMMANDKEYS};
  
  // Start package sent from the MUSIC side
  struct startPackage {
    int magicNumber;
    double stopTime;
  };

  // Data package sent to MUSIC
  struct toMusicPackage {
    double timestamp;
    double keysPressed[KEYBOARDSIZE];
    double commandKeys[COMMANDKEYS];
  };
  static constexpr int TOMUSICPORT = 9931;
  static constexpr int MAGICFROMMUSIC = 4712;
  static constexpr char const * MIDISERVER_IP = "130.237.221.78"; // wand.pdc.kth.se
  
  // Data package received from MUSIC
  struct fromMusicPackage {
    double timestamp;
    double keysPressed[KEYBOARDSIZE];
  };
  static constexpr int FROMMUSICPORT = 9930;
  static constexpr int MAGICTOMUSIC = 4711;
  
};
