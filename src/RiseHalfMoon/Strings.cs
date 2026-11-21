namespace RiseHalfMoon;

/// <summary>
/// Textos da interface nas 8 línguas do menu (os mesmos da versão PS Vita). Mudar de língua no menu
/// muda logo a interface e a língua dos jogos.
/// </summary>
public static class Strings
{
    // Ordem das colunas = ordem de GameCatalog.Languages: pt-PT, pt-BR, en, es, fr, de, it, nl.
    private static int _index = 2;

    public static string Code => GameCatalog.Languages[_index].Code;

    public static void SetLanguage(string code)
    {
        for (int i = 0; i < GameCatalog.Languages.Count; i++)
            if (GameCatalog.Languages[i].Code == code) { _index = i; return; }
    }

    /// <summary>Língua do Windows, se for uma das oferecidas; senão inglês.</summary>
    public static string DefaultCode()
    {
        var ui = System.Globalization.CultureInfo.CurrentUICulture;
        foreach (var (code, _) in GameCatalog.Languages)
            if (string.Equals(code, ui.Name, StringComparison.OrdinalIgnoreCase)) return code;
        if (ui.TwoLetterISOLanguageName == "pt") return "pt-PT";
        foreach (var (code, _) in GameCatalog.Languages)
            if (code == ui.TwoLetterISOLanguageName) return code;
        return "en";
    }

    public static string T(string key) => Table.TryGetValue(key, out var row) ? row[_index] : key;

    public static string F(string key, params object[] args) => string.Format(T(key), args);

    /// <summary>Nome do mês de cada jogo (1 = outubro … 8 = maio).</summary>
    public static string Month(int gameNumber) => Months[gameNumber - 1][_index];

    private static readonly string[][] Months =
    [
        ["Outubro", "Outubro", "October", "Octubre", "Octobre", "Oktober", "Ottobre", "Oktober"],
        ["Novembro", "Novembro", "November", "Noviembre", "Novembre", "November", "Novembre", "November"],
        ["Dezembro", "Dezembro", "December", "Diciembre", "Décembre", "Dezember", "Dicembre", "December"],
        ["Janeiro", "Janeiro", "January", "Enero", "Janvier", "Januar", "Gennaio", "Januari"],
        ["Fevereiro", "Fevereiro", "February", "Febrero", "Février", "Februar", "Febbraio", "Februari"],
        ["Março", "Março", "March", "Marzo", "Mars", "März", "Marzo", "Maart"],
        ["Abril", "Abril", "April", "Abril", "Avril", "April", "Aprile", "April"],
        ["Maio", "Maio", "May", "Mayo", "Mai", "Mai", "Maggio", "Mei"],
    ];

    private static readonly Dictionary<string, string[]> Table = new()
    {
        ["subtitle"] =
        [
            "Os 8 jogos Google Doodle, para jogar offline", "Os 8 jogos do Google Doodle, para jogar offline",
            "The 8 Google Doodle games, playable offline", "Los 8 juegos de Google Doodle, para jugar sin conexión",
            "Les 8 jeux Google Doodle, jouables hors ligne", "Die 8 Google-Doodle-Spiele, offline spielbar",
            "Gli 8 giochi Google Doodle, giocabili offline", "De 8 Google Doodle-spellen, offline speelbaar",
        ],
        ["language"] = ["Idioma", "Idioma", "Language", "Idioma", "Langue", "Sprache", "Lingua", "Taal"],
        ["download_update"] =
        [
            "Descarregar / atualizar jogos", "Baixar / atualizar jogos", "Download / update games",
            "Descargar / actualizar juegos", "Télécharger / mettre à jour", "Spiele laden / aktualisieren",
            "Scarica / aggiorna giochi", "Games downloaden / bijwerken",
        ],
        ["cancel"] = ["Cancelar", "Cancelar", "Cancel", "Cancelar", "Annuler", "Abbrechen", "Annulla", "Annuleren"],
        ["about"] = ["Acerca", "Sobre", "About", "Acerca de", "À propos", "Über", "Informazioni", "Over"],
        ["close"] = ["Fechar", "Fechar", "Close", "Cerrar", "Fermer", "Schließen", "Chiudi", "Sluiten"],
        ["available"] =
        [
            "Disponível offline", "Disponível offline", "Available offline", "Disponible sin conexión",
            "Disponible hors ligne", "Offline verfügbar", "Disponibile offline", "Offline beschikbaar",
        ],
        ["not_downloaded"] =
        [
            "Não descarregado", "Não baixado", "Not downloaded", "No descargado", "Non téléchargé", "Nicht geladen",
            "Non scaricato", "Niet gedownload",
        ],
        ["game_n_of_8"] =
        [
            "Jogo {0} de 8", "Jogo {0} de 8", "Game {0} of 8", "Juego {0} de 8", "Jeu {0} sur 8", "Spiel {0} von 8",
            "Gioco {0} di 8", "Spel {0} van 8",
        ],
        ["original"] = ["Original", "Original", "Original", "Original", "Original", "Original", "Originale", "Origineel"],
        ["all_available"] =
        [
            "Os 8 jogos estão guardados neste PC — não é precisa internet para jogar.",
            "Os 8 jogos estão salvos neste PC — não é preciso internet para jogar.",
            "All 8 games are saved on this PC — no internet needed to play.",
            "Los 8 juegos están guardados en este PC: no hace falta conexión para jugar.",
            "Les 8 jeux sont enregistrés sur ce PC — pas besoin d'internet pour jouer.",
            "Alle 8 Spiele sind auf diesem PC gespeichert — zum Spielen ist kein Internet nötig.",
            "Gli 8 giochi sono salvati su questo PC: non serve internet per giocare.",
            "Alle 8 spellen staan op deze pc — je hebt geen internet nodig om te spelen.",
        ],
        ["some_available"] =
        [
            "{0} de {1} jogos disponíveis offline. Com internet, use “{2}” para guardar os restantes.",
            "{0} de {1} jogos disponíveis offline. Com internet, use “{2}” para salvar os demais.",
            "{0} of {1} games available offline. With internet, use “{2}” to save the rest.",
            "{0} de {1} juegos disponibles sin conexión. Con internet, usa «{2}» para guardar el resto.",
            "{0} jeux sur {1} disponibles hors ligne. Avec internet, utilisez « {2} » pour enregistrer les autres.",
            "{0} von {1} Spielen offline verfügbar. Mit Internet „{2}“ verwenden, um den Rest zu speichern.",
            "{0} giochi su {1} disponibili offline. Con internet, usa «{2}» per salvare gli altri.",
            "{0} van {1} spellen offline beschikbaar. Gebruik met internet ‘{2}’ om de rest op te slaan.",
        ],
        ["open_folder"] =
        [
            "Abrir pasta dos jogos", "Abrir pasta dos jogos", "Open games folder", "Abrir carpeta de juegos",
            "Ouvrir le dossier des jeux", "Spieleordner öffnen", "Apri cartella dei giochi", "Map met spellen openen",
        ],
        ["menu"] = ["Menu", "Menu", "Menu", "Menú", "Menu", "Menü", "Menu", "Menu"],
        ["restart"] =
            ["Recomeçar", "Recomeçar", "Restart", "Reiniciar", "Recommencer", "Neu starten", "Ricomincia", "Opnieuw"],
        ["fullscreen"] =
        [
            "Ecrã inteiro (F11)", "Tela cheia (F11)", "Full screen (F11)", "Pantalla completa (F11)",
            "Plein écran (F11)", "Vollbild (F11)", "Schermo intero (F11)", "Volledig scherm (F11)",
        ],
        ["exit_fullscreen"] =
        [
            "Sair do ecrã inteiro (F11)", "Sair da tela cheia (F11)", "Exit full screen (F11)",
            "Salir de pantalla completa (F11)", "Quitter le plein écran (F11)", "Vollbild beenden (F11)",
            "Esci da schermo intero (F11)", "Volledig scherm sluiten (F11)",
        ],
        ["not_downloaded_title"] =
        [
            "Jogo não descarregado", "Jogo não baixado", "Game not downloaded", "Juego no descargado",
            "Jeu non téléchargé", "Spiel nicht geladen", "Gioco non scaricato", "Spel niet gedownload",
        ],
        ["ask_download"] =
        [
            "O jogo de {0} ainda não foi descarregado.\n\nDescarregar agora? Só é preciso internet desta vez.",
            "O jogo de {0} ainda não foi baixado.\n\nBaixar agora? Só é preciso internet desta vez.",
            "The {0} game hasn't been downloaded yet.\n\nDownload it now? An internet connection is only needed this once.",
            "El juego de {0} aún no se ha descargado.\n\n¿Descargarlo ahora? Solo necesitas conexión esta vez.",
            "Le jeu de {0} n'a pas encore été téléchargé.\n\nLe télécharger maintenant ? Une connexion n'est nécessaire qu'une fois.",
            "Das Spiel {0} wurde noch nicht geladen.\n\nJetzt herunterladen? Internet wird nur dieses eine Mal benötigt.",
            "Il gioco di {0} non è ancora stato scaricato.\n\nScaricarlo ora? La connessione serve solo questa volta.",
            "Het spel van {0} is nog niet gedownload.\n\nNu downloaden? Je hebt maar één keer internet nodig.",
        ],
        ["webview_missing_title"] =
        [
            "WebView2 em falta", "WebView2 ausente", "WebView2 missing", "Falta WebView2", "WebView2 manquant",
            "WebView2 fehlt", "WebView2 mancante", "WebView2 ontbreekt",
        ],
        ["webview_missing"] =
        [
            "O Microsoft Edge WebView2 Runtime não está instalado.\n\nInstale-o a partir de {0} e volte a abrir a app.",
            "O Microsoft Edge WebView2 Runtime não está instalado.\n\nInstale-o a partir de {0} e abra o app novamente.",
            "The Microsoft Edge WebView2 Runtime is not installed.\n\nInstall it from {0} and open the app again.",
            "Microsoft Edge WebView2 Runtime no está instalado.\n\nInstálalo desde {0} y vuelve a abrir la app.",
            "Microsoft Edge WebView2 Runtime n'est pas installé.\n\nInstallez-le depuis {0} puis rouvrez l'app.",
            "Die Microsoft Edge WebView2 Runtime ist nicht installiert.\n\nInstalliere sie von {0} und öffne die App erneut.",
            "Microsoft Edge WebView2 Runtime non è installato.\n\nInstallalo da {0} e riapri l'app.",
            "De Microsoft Edge WebView2 Runtime is niet geïnstalleerd.\n\nInstalleer hem via {0} en open de app opnieuw.",
        ],
        ["checking_game"] =
        [
            "A analisar o jogo de {0}…", "Analisando o jogo de {0}…", "Checking the {0} game…",
            "Analizando el juego de {0}…", "Analyse du jeu de {0}…", "Spiel {0} wird geprüft…",
            "Analisi del gioco di {0}…", "Spel van {0} controleren…",
        ],
        ["downloading_fonts"] =
        [
            "A descarregar fontes…", "Baixando fontes…", "Downloading fonts…", "Descargando fuentes…",
            "Téléchargement des polices…", "Schriften werden geladen…", "Download dei font…", "Lettertypen downloaden…",
        ],
        ["done"] = ["Concluído.", "Concluído.", "Done.", "Completado.", "Terminé.", "Fertig.", "Completato.", "Klaar."],
        ["dl_summary"] =
        [
            "Descarregados {0} ficheiros novos ({1} já existiam).", "Baixados {0} arquivos novos ({1} já existiam).",
            "Downloaded {0} new files ({1} were already there).", "Descargados {0} archivos nuevos ({1} ya existían).",
            "{0} nouveaux fichiers téléchargés ({1} existaient déjà).", "{0} neue Dateien geladen ({1} waren schon da).",
            "Scaricati {0} file nuovi ({1} erano già presenti).", "{0} nieuwe bestanden gedownload ({1} waren er al).",
        ],
        ["dl_errors"] =
        [
            " {0} erro(s).", " {0} erro(s).", " {0} error(s).", " {0} error(es).", " {0} erreur(s).", " {0} Fehler.",
            " {0} errore/i.", " {0} fout(en).",
        ],
        ["dl_failed_title"] =
        [
            "Alguns ficheiros falharam", "Alguns arquivos falharam", "Some files failed", "Algunos archivos fallaron",
            "Certains fichiers ont échoué", "Einige Dateien sind fehlgeschlagen", "Alcuni file non sono riusciti",
            "Sommige bestanden zijn mislukt",
        ],
        ["dl_cancelled"] =
        [
            "Descarga cancelada. O que já foi descarregado ficou guardado.",
            "Download cancelado. O que já foi baixado ficou salvo.",
            "Download cancelled. Everything downloaded so far was kept.",
            "Descarga cancelada. Lo ya descargado se ha guardado.",
            "Téléchargement annulé. Ce qui a déjà été téléchargé est conservé.",
            "Download abgebrochen. Bereits Geladenes bleibt gespeichert.",
            "Download annullato. Quanto già scaricato è stato conservato.",
            "Download geannuleerd. Wat al gedownload was, is bewaard.",
        ],
        ["dl_failed"] =
        [
            "Não foi possível descarregar: {0}", "Não foi possível baixar: {0}", "Could not download: {0}",
            "No se pudo descargar: {0}", "Téléchargement impossible : {0}", "Download nicht möglich: {0}",
            "Impossibile scaricare: {0}", "Downloaden mislukt: {0}",
        ],
        ["about_pc"] =
        [
            "Versão para PC", "Versão para PC", "PC version", "Versión para PC", "Version PC", "PC-Version",
            "Versione per PC", "Pc-versie",
        ],
        ["about_google"] =
        [
            "\"Rise of the Half Moon\" é uma série de jogos Google Doodle © Google LLC. Gráficos, música, sons, vídeo e " +
            "textos pertencem à Google: não estão incluídos nesta app e são descarregados dos servidores da Google.",
            "\"Rise of the Half Moon\" é uma série de jogos do Google Doodle © Google LLC. Gráficos, música, sons, vídeo e " +
            "textos pertencem ao Google: não estão incluídos neste app e são baixados dos servidores do Google.",
            "\"Rise of the Half Moon\" is a series of Google Doodle games © Google LLC. Graphics, music, sounds, video and " +
            "texts belong to Google: they are not included in this app and are downloaded from Google's servers.",
            "\"Rise of the Half Moon\" es una serie de juegos de Google Doodle © Google LLC. Gráficos, música, sonidos, " +
            "vídeo y textos pertenecen a Google: no se incluyen en esta app y se descargan de los servidores de Google.",
            "« Rise of the Half Moon » est une série de jeux Google Doodle © Google LLC. Graphismes, musique, sons, vidéo " +
            "et textes appartiennent à Google : ils ne sont pas inclus dans cette app et sont téléchargés depuis les " +
            "serveurs de Google.",
            "„Rise of the Half Moon“ ist eine Reihe von Google-Doodle-Spielen © Google LLC. Grafiken, Musik, Sounds, Video " +
            "und Texte gehören Google: Sie sind nicht in dieser App enthalten und werden von Googles Servern geladen.",
            "\"Rise of the Half Moon\" è una serie di giochi Google Doodle © Google LLC. Grafica, musica, suoni, video e " +
            "testi appartengono a Google: non sono inclusi in questa app e vengono scaricati dai server di Google.",
            "\"Rise of the Half Moon\" is een reeks Google Doodle-spellen © Google LLC. Graphics, muziek, geluiden, video " +
            "en teksten zijn van Google: ze zitten niet in deze app en worden gedownload van de servers van Google.",
        ],
        ["about_unofficial"] =
        [
            "Versão não oficial e sem fins comerciais, para uso pessoal e preservação. Sem qualquer ligação ou apoio da " +
            "Google ou da Microsoft.",
            "Versão não oficial e sem fins comerciais, para uso pessoal e preservação. Sem qualquer vínculo ou apoio do " +
            "Google ou da Microsoft.",
            "Unofficial, non-commercial fan port for personal use and preservation. Not affiliated with or endorsed by " +
            "Google or Microsoft.",
            "Versión no oficial y sin ánimo de lucro, para uso personal y preservación. Sin relación ni respaldo de Google " +
            "ni de Microsoft.",
            "Portage non officiel et non commercial, pour un usage personnel et la préservation. Ni affilié à Google ou à " +
            "Microsoft, ni approuvé par eux.",
            "Inoffizielle, nicht kommerzielle Fan-Portierung für den privaten Gebrauch und zur Bewahrung. Nicht mit Google " +
            "oder Microsoft verbunden oder von ihnen unterstützt.",
            "Versione non ufficiale e non commerciale, per uso personale e conservazione. Non affiliata né approvata da " +
            "Google o Microsoft.",
            "Onofficiële, niet-commerciële fanversie voor persoonlijk gebruik en behoud. Niet verbonden met of goedgekeurd " +
            "door Google of Microsoft.",
        ],
        ["third_party"] =
        [
            "Software de terceiros", "Software de terceiros", "Third-party software", "Software de terceros",
            "Logiciels tiers", "Software von Drittanbietern", "Software di terze parti", "Software van derden",
        ],
    };
}
