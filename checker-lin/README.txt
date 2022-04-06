	Turcitu Alexandru - Alin 334CC	

	Am definit structura in felul urmator:
fd -> retine file descriptor-ul
err -> contorizeaza numarul de erori
readBuffer -> buffer folosit la citire
writeBuffer -> buffer folosit la scriere
readIndex -> indexul de citire din buffer
writeIndex -> indexul de scriere din buffer
bytes_read, bytes_write -> folositi pentru operatiile de read/write
cursor -> pozitia cursorului in fisier
childPid, parentPid -> pid-urile necesare pentru wait in pclose
isPipe -> variabila necesara pentru a stii daca citim din pipe sau din fisier normal

	so_fopen:
Am initializat toate campurile si am deschis file descriptorul potrivit modului dat ca parametru.
Am alocat memorie pentru buffere si am returnat noua structura.

	so_fclose:
Daca exista ceva in buffer-ul de citire ii dam fflush(scriem tot in fisier), apoi inchide file descriptor-ul
si dezalocam memoria atat pentru buffere cat si pentru structura.

	so_fflush:
Ca si in laboratorul 2, facem scrierea intr-un while pentru a ne asigura ca scriem tot continutul buffer-ului,
iar la final golim buffer-ul.

	so_fseek:
Pur si simplu apelam lseek cu parametrii oferiti si setam cursorul din structura

	so_fgetc:
Citim in buffer doar daca buffer-ul este gol sau plin (caz in care setam indexul de citire pe 0) si returnam 
valoarea din buffer de la pozitia indexului (proces care are loc chiar daca nu re-populam buffer-ul).

	so_fread:
For in for (nmemb elemente de dimensiune size), la fiecare pas apelam fgetc pe structura si punem valoarea
in pointer-ul dat ca parametru. In implementarea mea, din anumite cauze, lseek pe orice file descriptor din
pipe ar returna valoarea 0 drept dimensiunea fisierului, astfel a trebui sa integrez o exceptie in fread din
pipe, si anume: cand ajung la sfarsitul fisierului cu citirea, setez cursorul pe 0 si flag-ul de pipe pe 0
pentru a putea trece mai tarziu de so_feof.

	so_fputc:
Punem in buffer-ul de scriere valoarea primita drept parametru si dam flush la buffer daca s-a umplut.

	so_fwrite:
Ca si al fread, dar in loc sa populam pointer-ul dat ca parametru cu valori din buffer, facem pe dos.

	so_feof:
Facem diferenta intre cursor si dimensiunea fisierului, in cazul in care sunt egale inseamna ca am ajuns
la final. Aici este si exceptia legata de pipe mentionata anterior.

	so_popen:
Facem pipe apoi fork (care daca esueaza returnam NULL). Daca fork-ul esueaza inchidem file descriptorii si 
intoarcem NULL. In procesul copil am procedat in felul urmator: la type = read am redirectat iesirea standard in pipe-ul de scriere, iar la type = write am redirectat intrarea standard in pipe-ul de citire, iar in ambele cazuri am inchis ambii file descriptori. In procesul parinte am procedat pe dos: type == read, duplicam pipe-ul de citire si vice-versa. In continuare am initializat si alocat corespunzator membrii structurii si am retinut totodata pid-ul copilului si pid-ul procesului curent pentru a face wait in pclose.

	so_pclose:
Inchidem file descriptorul ramas (cu fclose) si facem wait atat pe procesul copil cat si pe parinte. In functie de
status, returnam -1 sau 0.