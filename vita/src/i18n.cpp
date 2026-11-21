#include "i18n.h"

#include <array>
#include <map>

#include "app.h"
#include "catalog.h"

namespace i18n {

namespace {

// Ordem das colunas: en, pt-PT, pt-BR, es, fr, de, it, nl (igual a catalog::languages()).
const int N = 8;

const std::map<std::string, std::array<const char*, N>>& table() {
  static const std::map<std::string, std::array<const char*, N>> t = {
      {"subtitle",
       {"The 8 Google Doodle games, playable offline", "Os 8 jogos Google Doodle, para jogar offline",
        "Os 8 jogos do Google Doodle, para jogar offline", "Los 8 juegos de Google Doodle, para jugar sin conexión",
        "Les 8 jeux Google Doodle, jouables hors ligne", "Die 8 Google-Doodle-Spiele, offline spielbar",
        "Gli 8 giochi Google Doodle, giocabili offline", "De 8 Google Doodle-spellen, offline speelbaar"}},
      {"landscape", {"Landscape", "Horizontal", "Horizontal", "Horizontal", "Paysage", "Querformat", "Orizzontale", "Liggend"}},
      {"portrait", {"Portrait", "Vertical", "Vertical", "Vertical", "Portrait", "Hochformat", "Verticale", "Staand"}},
      {"download_update",
       {"Download / update games", "Descarregar / atualizar jogos", "Baixar / atualizar jogos",
        "Descargar / actualizar juegos", "Télécharger / mettre à jour", "Spiele laden / aktualisieren",
        "Scarica / aggiorna giochi", "Games downloaden / bijwerken"}},
      {"cancel", {"Cancel", "Cancelar", "Cancelar", "Cancelar", "Annuler", "Abbrechen", "Annulla", "Annuleren"}},
      {"coming_soon", {"Coming soon", "Brevemente", "Em breve", "Próximamente", "Bientôt", "Demnächst", "Prossimamente", "Binnenkort"}},
      {"available",
       {"Available offline", "Disponível offline", "Disponível offline", "Disponible sin conexión",
        "Disponible hors ligne", "Offline verfügbar", "Disponibile offline", "Offline beschikbaar"}},
      {"not_downloaded",
       {"Not downloaded", "Não descarregado", "Não baixado", "No descargado", "Non téléchargé", "Nicht geladen",
        "Non scaricato", "Niet gedownload"}},
      {"language", {"Language", "Idioma", "Idioma", "Idioma", "Langue", "Sprache", "Lingua", "Taal"}},
      {"ask_download",
       {"This game hasn't been downloaded yet. Download it now? An internet connection is only needed this once.",
        "Este jogo ainda não foi descarregado. Descarregar agora? Só é preciso internet desta vez.",
        "Este jogo ainda não foi baixado. Baixar agora? Só é preciso internet desta vez.",
        "Este juego aún no se ha descargado. ¿Descargarlo ahora? Solo necesitas conexión esta vez.",
        "Ce jeu n'a pas encore été téléchargé. Le télécharger maintenant ? Une connexion n'est nécessaire qu'une fois.",
        "Dieses Spiel wurde noch nicht geladen. Jetzt herunterladen? Internet wird nur dieses eine Mal benötigt.",
        "Questo gioco non è ancora stato scaricato. Scaricarlo ora? La connessione serve solo questa volta.",
        "Dit spel is nog niet gedownload. Nu downloaden? Je hebt maar één keer internet nodig."}},
      {"download", {"Download", "Descarregar", "Baixar", "Descargar", "Télécharger", "Herunterladen", "Scarica", "Downloaden"}},
      {"not_supported",
       {"{0} is not supported yet.", "{0} ainda não é suportado.", "{0} ainda não é suportado.",
        "{0} aún no está disponible.", "{0} n'est pas encore pris en charge.", "{0} wird noch nicht unterstützt.",
        "{0} non è ancora supportato.", "{0} wordt nog niet ondersteund."}},
      {"dl_complete",
       {"Download complete.", "Download concluído.", "Download concluído.", "Descarga completada.",
        "Téléchargement terminé.", "Download abgeschlossen.", "Download completato.", "Download voltooid."}},
      {"dl_cancelled",
       {"Download cancelled.", "Download cancelado.", "Download cancelado.", "Descarga cancelada.",
        "Téléchargement annulé.", "Download abgebrochen.", "Download annullato.", "Download geannuleerd."}},
      {"dl_failed",
       {"Some files could not be downloaded.", "Não foi possível descarregar alguns ficheiros.",
        "Não foi possível baixar alguns arquivos.", "No se pudieron descargar algunos archivos.",
        "Certains fichiers n'ont pas pu être téléchargés.", "Einige Dateien konnten nicht geladen werden.",
        "Alcuni file non sono stati scaricati.", "Sommige bestanden konden niet worden gedownload."}},
      {"quit_question",
       {"Quit to the main menu? Your current game will be lost.",
        "Voltar ao menu principal? O jogo atual será perdido.", "Voltar ao menu principal? O jogo atual será perdido.",
        "¿Volver al menú principal? Se perderá la partida actual.",
        "Revenir au menu principal ? La partie en cours sera perdue.",
        "Zurück zum Hauptmenü? Das aktuelle Spiel geht verloren.",
        "Tornare al menu principale? La partita in corso andrà persa.",
        "Terug naar het hoofdmenu? Het huidige spel gaat verloren."}},
      {"quit", {"Quit", "Sair", "Sair", "Salir", "Quitter", "Beenden", "Esci", "Stoppen"}},
      {"keep_playing",
       {"Keep playing", "Continuar a jogar", "Continuar jogando", "Seguir jugando", "Continuer", "Weiterspielen",
        "Continua a giocare", "Verder spelen"}},
      {"about", {"About", "Acerca", "Sobre", "Acerca de", "À propos", "Über", "Informazioni", "Over"}},
      {"close", {"Close", "Fechar", "Fechar", "Cerrar", "Fermer", "Schließen", "Chiudi", "Sluiten"}},
      {"about_port",
       {"PS Vita port", "Versão para PS Vita", "Versão para PS Vita", "Versión para PS Vita", "Portage PS Vita",
        "PS-Vita-Portierung", "Versione per PS Vita", "PS Vita-versie"}},
      {"about_google",
       {"\"Rise of the Half Moon\" is a series of Google Doodle games © Google LLC. Graphics, music, sounds, video and "
        "texts belong to Google: they are not included in this app and are downloaded from Google's servers.",
        "\"Rise of the Half Moon\" é uma série de jogos Google Doodle © Google LLC. Gráficos, música, sons, vídeo e "
        "textos pertencem à Google: não estão incluídos nesta app e são descarregados dos servidores da Google.",
        "\"Rise of the Half Moon\" é uma série de jogos do Google Doodle © Google LLC. Gráficos, música, sons, vídeo e "
        "textos pertencem ao Google: não estão incluídos neste app e são baixados dos servidores do Google.",
        "\"Rise of the Half Moon\" es una serie de juegos de Google Doodle © Google LLC. Gráficos, música, sonidos, "
        "vídeo y textos pertenecen a Google: no se incluyen en esta app y se descargan de los servidores de Google.",
        "« Rise of the Half Moon » est une série de jeux Google Doodle © Google LLC. Graphismes, musique, sons, vidéo "
        "et textes appartiennent à Google : ils ne sont pas inclus dans cette app et sont téléchargés depuis les "
        "serveurs de Google.",
        "„Rise of the Half Moon“ ist eine Reihe von Google-Doodle-Spielen © Google LLC. Grafiken, Musik, Sounds, Video "
        "und Texte gehören Google: Sie sind nicht in dieser App enthalten und werden von Googles Servern geladen.",
        "\"Rise of the Half Moon\" è una serie di giochi Google Doodle © Google LLC. Grafica, musica, suoni, video e "
        "testi appartengono a Google: non sono inclusi in questa app e vengono scaricati dai server di Google.",
        "\"Rise of the Half Moon\" is een reeks Google Doodle-spellen © Google LLC. Graphics, muziek, geluiden, video "
        "en teksten zijn van Google: ze zitten niet in deze app en worden gedownload van de servers van Google."}},
      {"about_unofficial",
       {"Unofficial, non-commercial fan port for personal use and preservation. Not affiliated with or endorsed by "
        "Google or Sony Interactive Entertainment.",
        "Versão não oficial e sem fins comerciais, para uso pessoal e preservação. Sem qualquer ligação ou apoio da "
        "Google ou da Sony Interactive Entertainment.",
        "Versão não oficial e sem fins comerciais, para uso pessoal e preservação. Sem qualquer vínculo ou apoio do "
        "Google ou da Sony Interactive Entertainment.",
        "Versión no oficial y sin ánimo de lucro, para uso personal y preservación. Sin relación ni respaldo de Google "
        "ni de Sony Interactive Entertainment.",
        "Portage non officiel et non commercial, pour un usage personnel et la préservation. Ni affilié à Google ou à "
        "Sony Interactive Entertainment, ni approuvé par eux.",
        "Inoffizielle, nicht kommerzielle Fan-Portierung für den privaten Gebrauch und zur Bewahrung. Nicht mit Google "
        "oder Sony Interactive Entertainment verbunden oder von ihnen unterstützt.",
        "Versione non ufficiale e non commerciale, per uso personale e conservazione. Non affiliata né approvata da "
        "Google o Sony Interactive Entertainment.",
        "Onofficiële, niet-commerciële fanversie voor persoonlijk gebruik en behoud. Niet verbonden met of goedgekeurd "
        "door Google of Sony Interactive Entertainment."}},
      {"third_party",
       {"Third-party software", "Software de terceiros", "Software de terceiros", "Software de terceros",
        "Logiciels tiers", "Software von Drittanbietern", "Software di terze parti", "Software van derden"}},
  };
  return t;
}

const std::map<std::string, std::array<const char*, N>>& months() {
  // outubro..maio (índice do jogo 0..7)
  static const std::map<std::string, std::array<const char*, N>> m = {
      {"0", {"October", "Outubro", "Outubro", "Octubre", "Octobre", "Oktober", "Ottobre", "Oktober"}},
      {"1", {"November", "Novembro", "Novembro", "Noviembre", "Novembre", "November", "Novembre", "November"}},
      {"2", {"December", "Dezembro", "Dezembro", "Diciembre", "Décembre", "Dezember", "Dicembre", "December"}},
      {"3", {"January", "Janeiro", "Janeiro", "Enero", "Janvier", "Januar", "Gennaio", "Januari"}},
      {"4", {"February", "Fevereiro", "Fevereiro", "Febrero", "Février", "Februar", "Febbraio", "Februari"}},
      {"5", {"March", "Março", "Março", "Marzo", "Mars", "März", "Marzo", "Maart"}},
      {"6", {"April", "Abril", "Abril", "Abril", "Avril", "April", "Aprile", "April"}},
      {"7", {"May", "Maio", "Maio", "Mayo", "Mai", "Mai", "Maggio", "Mei"}},
  };
  return m;
}

int langIndex() {
  const std::string& code = app::settings().language;
  auto& langs = catalog::languages();
  for (size_t i = 0; i < langs.size() && i < (size_t)N; i++)
    if (langs[i].code == code) return (int)i;
  return 0;
}

}  // namespace

const std::string& tr(const char* key) {
  static std::map<std::string, std::string> cache[N];
  int li = langIndex();
  auto it = cache[li].find(key);
  if (it != cache[li].end()) return it->second;
  auto& t = table();
  auto row = t.find(key);
  std::string v = row == t.end() ? key : row->second[li];
  return cache[li].emplace(key, v).first->second;
}

std::string fmt(const char* key, const std::string& arg) {
  std::string s = tr(key);
  auto p = s.find("{0}");
  if (p != std::string::npos) s.replace(p, 3, arg);
  return s;
}

std::string monthName(int gameIndex) {
  auto it = months().find(std::to_string(gameIndex));
  return it == months().end() ? "" : it->second[langIndex()];
}

std::string gameTitle(int gameIndex) {
  const auto& g = catalog::games()[gameIndex];
  return monthName(gameIndex) + " " + g.title.substr(g.title.size() - 4);
}

}  // namespace i18n
