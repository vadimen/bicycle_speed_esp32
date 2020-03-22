Plan general pt aplicatie: 
Ce face aplicatia esp32 ? 
    1. Citeste prin intrerupere semnal de la senzor [facut]
    2. calculeaza viteza bicicletei in raport cu parametri(distanta senzorului de la centru rotii) [facut]
    3. transmite datele prin wifi la smartphone[facut]
    4. primeste de la smartphone setarile atunci cand ele se modifica[facut]

ce face aplicatia android ?
    1. Se conecteaza la esp32[facut]
    2. afiseaza viteza primita in realtime de la esp32[facut]
    3. Are campuri pentru a seta niste parametri

Imbunatatiri:
-telefonul va fi hotspot pt ca sa mai poata folsi si date mobile [facut]
-comunicare prin web socket sau tcp [facut]
-de ajustat timpul in dependenta de pozitia senzorului pe roata [facut]
-de facut ca aplicatia android sa trimita request la /input.html?25 ca sa seteze raza rotii (pt vers veche cu html) [facut]

Descriere pt v1 al aplicatiei:
	esp32 primeste prin intrerupere la pin un semnal de la hall senzor
	in versiunea asta esp32 este wifi hotspot si html server
	aplicatia android descarca mereu pagina html si scoate din ea viteza
	pt a trimite raza rotii aplicatia android face un request pe /input.html?42
	unde 42 e setarea, apoi esp32 scoate acest 42 din insusi request