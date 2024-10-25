#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Matrice des boutons (conservation des broches d'origine)
const int ROW_PINS[] = {7, 8, 1};  // Broches de lignes pour la matrice de boutons
const int COL_PINS[] = {20, 10, 0};  // Broches de colonnes pour la matrice de boutons

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Variables pour le contrôle du pixel
int pixelX = SCREEN_WIDTH / 2;
int pixelY = SCREEN_HEIGHT / 2;
int enemyX = 10;
int enemyY = 20;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200; // Délai de debounce en ms

// Variables pour le combat
int playerHealth = 10;
int enemyHealth = 10;
bool inCombat = false;
bool gameEnded = false;
bool playerWon = false;
bool inTradeMenu = false; // Pour indiquer si on est dans le menu d'échange

const int speakerPin = 3; // Pin où le haut-parleur est connecté
unsigned long lastNoteTime = 0;
int currentNote = 0;

// Structure de définition pour les objets
struct Item {
  const char* name;
  int effectPower; // Puissance de l'effet (ex: soin en PV)
  const char* effectType; // Type d'effet (ex: "heal", "boost")
};

Item inventory[] = {
  {"Potion", 5, "heal"},        // Potion qui soigne 5 PV
  {"Super Potion", 10, "heal"}, // Potion qui soigne 10 PV
  {"Boost", 3, "boost"}         // Boost qui pourrait améliorer les attaques
};
int inventorySize = sizeof(inventory) / sizeof(inventory[0]);

// Structure de définition pour les monstres
struct Monster {
  const char* name;
  int power;   // Puissance de l'attaque
  const char* type; // Type de monstre (ex: "Feu", "Eau")
  int health;  // Vie du monstre
};

Monster monsterInventory[5] = {
  {"Flamino", 4, "Feu", 10},
  {"Aquara", 3, "Eau", 12}
};
int monsterCount = 2; // Nombre initial de monstres

// Fréquences des notes (en Hz)
const int notes[] = { 
  523, 392, 440, 523, 440, 392, // Intro
  523, 392, 440, 523, 440, 392, 
  330, 294, 330, 349, 392, 440, // Partie suivante
  330, 294, 330, 349, 392, 440, 
  523, 392, 440, 523, 440, 392, // Reprise de l'intro
  523, 392, 440, 523, 440, 392,
  330, 294, 330, 349, 392, 440, // Partie suivante répétée
  330, 294, 330, 349, 392, 440  
};

// Durées des notes (en ms)
const int durations[] = { 
  250, 250, 250, 250, 250, 250, // Durées pour l'intro
  250, 250, 250, 250, 250, 250,
  250, 250, 250, 250, 250, 250, // Durées pour la partie suivante
  250, 250, 250, 250, 250, 250,
  250, 250, 250, 250, 250, 250, // Durées pour la reprise de l'intro
  250, 250, 250, 250, 250, 250,
  250, 250, 250, 250, 250, 250, // Partie suivante répétée
  250, 250, 250, 250, 250, 250  
};

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, 2, 3); // Communication série pour échanges avec d'autres ESP32
  
  // Initialisation de l'écran OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true);
  }
  display.clearDisplay();
  drawPixel();
  
  // Configuration des boutons (matrice)
  setupButtonMatrix();
}

void loop() {
  // Gérer la musique
  playMusic();

  // Gestion du combat et des échanges
  if (inCombat && !gameEnded) {
    displayCombat();
  } else if (gameEnded) {
    displayEndScreen();
  } else if (inTradeMenu) {
    handleTradeMenu(); // Gérer le menu d'échange
  } else {
    handleButtonPress();
  }
  receiveMonster();
}

// Fonction pour configurer la matrice de boutons
void setupButtonMatrix() {
  for (int row = 0; row < 3; row++) {
    pinMode(ROW_PINS[row], OUTPUT);
    digitalWrite(ROW_PINS[row], HIGH); // Activer les lignes à HIGH
  }
  
  for (int col = 0; col < 3; col++) {
    pinMode(COL_PINS[col], INPUT_PULLUP); // Entrée avec résistance de tirage interne
  }
}

// Gestion de la pression des boutons avec debounce
void handleButtonPress() {
  for (int row = 0; row < 3; row++) {
    digitalWrite(ROW_PINS[row], LOW); // Activer la ligne actuelle

    for (int col = 0; col < 3; col++) {
      if (digitalRead(COL_PINS[col]) == LOW) {
        if (millis() - lastDebounceTime > debounceDelay) {
          lastDebounceTime = millis();
          if (!inCombat && !inTradeMenu) {
            if (row == 1 && col == 1) { // Bouton pour ouvrir le menu d'échange
              openTradeMenu();
            } else {
              movePixel(row, col);
              checkForCombat();
            }
          } else if (gameEnded) {
            resetGame(); // Réinitialiser si le jeu est terminé et un bouton est pressé
          } else if (inCombat) {
            handleCombatInput(row, col);
          }
        }
      }
    }
    digitalWrite(ROW_PINS[row], HIGH); // Désactiver la ligne actuelle
  }
}

// Fonction pour ouvrir le menu d'échange
void openTradeMenu() {
  inTradeMenu = true;
  displayTradeMenu();
}

// Fonction pour afficher le menu d'échange
void displayTradeMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.setTextColor(SSD1306_WHITE);
  display.print("Mes monstres :");
  display.setCursor(0, 10);
  for (int i = 0; i < 2; i++) {
    display.setCursor(0, 10 + (i * 10));
    display.print(i+1);
    display.print(": ");
    display.print(monsterInventory[i].name);
  }
  display.setCursor(0, 50);
  display.print("A: Echange  B: Retour");
  display.display();
}

// Gérer les choix dans le menu d'échange
void handleTradeMenu() {
  for (int row = 0; row < 3; row++) {
    digitalWrite(ROW_PINS[row], LOW);
    for (int col = 0; col < 3; col++) {
      if (digitalRead(COL_PINS[col]) == LOW) {
        if (millis() - lastDebounceTime > debounceDelay) {
          lastDebounceTime = millis();
          if (row == 0 && col == 2) { // Bouton pour envoyer un monstre (colonne de "Start")
            chooseMonsterToSend();
          } else if (row == 1 && col == 2) { // Retourner à l'écran principal
            inTradeMenu = false;
            drawPixel();
          }
        }
      }
    }
    digitalWrite(ROW_PINS[row], HIGH); // Désactiver la ligne actuelle
  }
}

// Fonction pour choisir un monstre à envoyer
void chooseMonsterToSend() {
  int selectedMonster = 0;
  bool selecting = true;

  delay(300);

  while (selecting) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Choisir le Monstre:");
    
    // Afficher jusqu'à 3 monstres à la fois pour une navigation plus facile
    for (int i = 0; i < 3; i++) {
      int index = selectedMonster + i;
      if (index >= monsterCount) break; // Si on dépasse le nombre de monstres
      display.setCursor(0, 20 + (i * 10));
      if (i == 0) {
        display.print("-> "); // Indicateur de sélection
      } else {
        display.print("   "); // Pas d'indicateur
      }
      display.print(monsterInventory[index].name);
    }

    display.display();

    for (int row = 0; row < 3; row++) {
      digitalWrite(ROW_PINS[row], LOW);
      for (int col = 0; col < 3; col++) {
        if (digitalRead(COL_PINS[col]) == LOW) {
          delay(200); // Pour éviter les actions trop rapides
          
          if (row == 0 && col == 0) { // Monter dans la liste
            if (selectedMonster > 0) {
              selectedMonster--;
            }
          } 
          else if (row == 2 && col == 0) { // Descendre dans la liste
            if (selectedMonster < monsterCount - 1) {
              selectedMonster++;
            }
          } 
          else if (row == 0 && col == 2) { // "Start" pour confirmer l'envoi
            display.clearDisplay();
            display.setCursor(0, 0);
            display.print("Envoi : ");
            display.print(monsterInventory[selectedMonster].name);
            display.display();
            delay(1000); // Petite pause pour confirmation visuelle
            
            sendMonster(monsterInventory[selectedMonster].name);
            selecting = false;
            inTradeMenu = false;
          } 
          else if (row == 2 && col == 2) { // Bouton pour annuler et revenir
            selecting = false;
            inTradeMenu = false;
            drawPixel();
          }
        }
      }
      digitalWrite(ROW_PINS[row], HIGH);
    }
  }
}

// Fonction pour envoyer un monstre via Serial1
void sendMonster(const char* monster) {
  // Trouver le monstre dans l'inventaire pour envoyer ses données complètes
  for (int i = 0; i < monsterCount; i++) {
    if (strcmp(monsterInventory[i].name, monster) == 0) {
      // Envoyer les données complètes du monstre via Serial1
      Serial1.print(monsterInventory[i].name);
      Serial1.print(",");
      Serial1.print(monsterInventory[i].power);
      Serial1.print(",");
      Serial1.print(monsterInventory[i].type);
      Serial1.print(",");
      Serial1.println(monsterInventory[i].health);

      Serial.print(monsterInventory[i].name);
      Serial.print(",");
      Serial.print(monsterInventory[i].power);
      Serial.print(",");
      Serial.print(monsterInventory[i].type);
      Serial.print(",");
      Serial.println(monsterInventory[i].health);

      // Afficher le message de confirmation
      displayMessage("Monstre envoye! \n A: Fini");

      // Retirer le monstre de l'inventaire après l'envoi
      for (int j = i; j < monsterCount - 1; j++) {
        monsterInventory[j] = monsterInventory[j + 1];
      }
      monsterCount--; // Réduire le nombre total de monstres
      
      break;
    }
  }
}

// Fonction pour recevoir un monstre
void receiveMonster() {
  Serial.print("Receive");
  String receivedData = "";  // Variable statique pour accumuler les données
  if (Serial1.available()) {
    char c = Serial1.read();  // Lire caractère par caractère
    if (c == '\n') {
      Serial.println("Received");
      // Lorsque la fin de ligne est atteinte, traiter les données
      parseMonsterData(receivedData);
      receivedData = "";  // Réinitialiser pour le prochain message
    } else {
      receivedData += c;  // Accumuler les caractères
    }
    delay(100);
  }
}

void parseMonsterData(const String &data) {
  // Parse les données reçues pour créer un nouveau monstre
  int firstComma = data.indexOf(',');
  int secondComma = data.indexOf(',', firstComma + 1);
  int thirdComma = data.indexOf(',', secondComma + 1);

  if (firstComma != -1 && secondComma != -1 && thirdComma != -1) {
    String name = data.substring(0, firstComma);
    int power = data.substring(firstComma + 1, secondComma).toInt();
    String type = data.substring(secondComma + 1, thirdComma);
    int health = data.substring(thirdComma + 1).toInt();

    Serial.print("Nom : "); Serial.println(name);
    Serial.print("Puissance : "); Serial.println(power);
    Serial.print("Type : "); Serial.println(type);
    Serial.print("Vie : "); Serial.println(health);

    if (monsterCount < 5) {
      Monster newMonster;
      newMonster.name = strdup(name.c_str());  // Assurez-vous de copier la chaîne
      newMonster.power = power;
      newMonster.type = strdup(type.c_str());  // Assurez-vous de copier la chaîne
      newMonster.health = health;

      // Ajouter le nouveau monstre à l'inventaire
      monsterInventory[monsterCount++] = newMonster;
      displayMessage("Monstre reçu!");
    } else {
      displayMessage("Inventaire plein!");
    }
  }
}


// Fonction pour afficher un message temporaire
void displayMessage(const char* msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print(msg);
  display.display();
  delay(1000);
}

// Fonction de déplacement du pixel
void movePixel(int row, int col) {
  if (row == 0 && col == 0) { // Haut
    if (pixelY > 0) pixelY--; 
  } else if (row == 2 && col == 0) { // Bas
    if (pixelY < SCREEN_HEIGHT - 1) pixelY++; 
  } else if (row == 1 && col == 0) { // Gauche
    if (pixelX > 0) pixelX--; 
  } else if (row == 0 && col == 1) { // Droite
    if (pixelX < SCREEN_WIDTH - 1) pixelX++; 
  }
  drawPixel();
}

// Afficher les pixels du joueur et de l'ennemi
void drawPixel() {
  display.clearDisplay();
  display.drawPixel(pixelX, pixelY, SSD1306_WHITE);
  display.drawPixel(enemyX, enemyY, SSD1306_WHITE);
  display.display();
}

// Vérifier si le joueur rencontre l'ennemi pour commencer le combat
void checkForCombat() {
  if (pixelX == enemyX && pixelY == enemyY) {
    inCombat = true;
    display.clearDisplay();
    display.display();
  }
}

// Fonction pour utiliser un objet en combat
void useItem(int itemIndex) {
  if (itemIndex >= 0 && itemIndex < inventorySize) {
    if (strcmp(inventory[itemIndex].effectType, "heal") == 0) {
      playerHealth += inventory[itemIndex].effectPower;
      displayMessage("Utilise: ");
      displayMessage(inventory[itemIndex].name);
    }
    checkCombatEnd();
  }
}

// Gérer la sélection d'action en combat (attaque ou utilisation d'objets)
void handleCombatInput(int row, int col) {
  if (row == 0 && col == 2) { // Attaque 1
    playerAttack(1);
  } else if (row == 2 && col == 2) { // Attaque 2
    playerAttack(2);
  } else if (row == 1 && col == 1) { // Utiliser objet
    useItem(0); // Exemple : utiliser la première potion
  }
  enemyTurn();
}

// Fonction pour les attaques du joueur
void playerAttack(int attackType) {
  int damage = 0;
  int hitChance = random(0, 100);
  if (attackType == 1) {
    if (hitChance < 80) {
      damage = 2;
      enemyHealth -= damage;
      displayMessage("Attaque A! -2 PV");
    } else {
      displayMessage("Attaque ratee!");
    }
  } else if (attackType == 2) {
    if (hitChance < 50) {
      damage = 6;
      enemyHealth -= damage;
      displayMessage("Attaque B! -6 PV");
    } else {
      displayMessage("Attaque ratee!");
    }
  }
  checkCombatEnd();
}

// Tour de l'ennemi
void enemyTurn() {
  int attackType = random(1, 3); 
  int damage = 0;
  int hitChance = random(0, 100);
  
  if (attackType == 1) {
    if (hitChance < 70) {
      damage = 2;
      playerHealth -= damage;
      displayMessage("Ennemi Attaque A! -2 PV");
    } else {
      displayMessage("Ennemi ratee!");
    }
  } else if (attackType == 2) {
    if (hitChance < 40) {
      damage = 3;
      playerHealth -= damage;
      displayMessage("Ennemi Attaque B! -3 PV");
    } else {
      displayMessage("Ennemi ratee!");
    }
  }
  checkCombatEnd();
}

// Vérifier la fin du combat
void checkCombatEnd() {
  if (playerHealth <= 0) {
    displayMessage("Vous avez perdu!");
    gameEnded = true;
    playerWon = false;
  } else if (enemyHealth <= 0) {
    displayMessage("Vous avez gagne!");
    gameEnded = true;
    playerWon = true;
  }
}

// Affichage du combat
void displayCombat() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.setTextColor(SSD1306_WHITE);
  display.print("Joueur PV: ");
  display.print(playerHealth);
  display.setCursor(0, 10);
  display.print("Ennemi PV: ");
  display.print(enemyHealth);
  display.setCursor(0, 30);
  display.print("A: Charge (2,chance+)");
  display.setCursor(0, 40);
  display.print("B: Laser  (6,chance-)");
  display.setCursor(0, 50);
  display.print("Objet: ");
  display.print(inventory[0].name); 
  display.display();
}

// Afficher l'écran de fin avec animation de victoire ou défaite
void displayEndScreen() {
  display.clearDisplay();
  if (playerWon) {
    for (int i = 0; i < 50; i++) {
      int x = random(0, SCREEN_WIDTH);
      int y = random(0, SCREEN_HEIGHT);
      display.drawPixel(x, y, SSD1306_WHITE);
      display.display();
      delay(20);
    }
    delay(1000);
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(20, 20);
    display.print("Victoire!");
    display.display();
  } else {
    display.setTextSize(2);
    display.setCursor(20, 20);
    display.print("Defaite!");
    display.display();
  }
  while (true) {
    for (int row = 0; row < 3; row++) {
      digitalWrite(ROW_PINS[row], LOW);
      for (int col = 0; col < 3; col++) {
        if (digitalRead(COL_PINS[col]) == LOW) {
          resetGame();
          return;
        }
      }
      digitalWrite(ROW_PINS[row], HIGH);
    }
  }
}

// Réinitialiser après le combat
void resetGame() {
  playerHealth = 10;
  enemyHealth = 10;
  inCombat = false;
  gameEnded = false;
  pixelX = SCREEN_WIDTH / 2;
  pixelY = SCREEN_HEIGHT / 2;
  drawPixel();
}

// Fonction pour jouer la musique
void playMusic() {
  unsigned long currentTime = millis();
  if (currentNote < sizeof(notes) / sizeof(notes[0])) {
    if (currentTime - lastNoteTime >= durations[currentNote]) {
      tone(speakerPin, notes[currentNote]);
      lastNoteTime = currentTime;
      currentNote++;
    }
  } else {
    currentNote = 0;
  }
}