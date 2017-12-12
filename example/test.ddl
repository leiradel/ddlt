// The weapons available in the game
enum Weapons {
  kFist,
  kChainsaw,
  kPistol,
  kShotgun,
  kChaingun,
  kRocketLauncher,
  kPlasmaGun,
  kBFG9000
};

/* The player */
struct Hero {
  string name = "Hero";
  int health = 100;
  int armour = 0x0;
  float speed = 14.3;
  isAlive = [{
    return true;
  }]
};
