# GuessTheNumber
GuessTheNumber is a number guessing game that supports multiple clients connecting to the game server through Windows Mailslots. Each client can guess a number between 0
and 255 as often and whenever they want to. The first client to guess the correct number wins.

GuessTheNumber uses multithreading to execute the multiple server and client tasks that are required to be run in parllel; shared memory as the mechanism that
allows the the clients to to send guesses and the server to read them; and an event to signal to every client when the number has been guessed correctly.
