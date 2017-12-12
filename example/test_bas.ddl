' The weapons available in the game
enum Weapons as int
  kFist
  kChainsaw
  kPistol
  kShotgun
  kChaingun
  kRocketLauncher
  kPlasmaGun
  kBFG9000
end enum

rem The player
structure Hero
  name as string = "John ""Hero"" Doe"
  health as int = 100%
  armour as int = &h0
  speed as float = 14.3
  isAlive = [{
    return true;
  }]
end structure
