#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "input_config.h"
#include "prompt_control.h"

namespace dwg2::recomp_menu {

inline constexpr int kLangEnglish = 1;
inline constexpr int kLangGerman = 3;
inline constexpr int kLangFrench = 4;
inline constexpr int kLangSpanish = 5;
inline constexpr int kLangItalian = 6;

inline constexpr int kLanguageOrder[5] = {
    kLangEnglish, kLangFrench, kLangGerman, kLangItalian, kLangSpanish};

inline constexpr bool ValidLanguage(int lang) {
  return lang == kLangEnglish || lang == kLangGerman || lang == kLangFrench ||
         lang == kLangSpanish || lang == kLangItalian;
}

inline constexpr int NormalizeLanguage(int lang) {
  return ValidLanguage(lang) ? lang : kLangEnglish;
}

inline constexpr int LanguageIndex(int lang) {
  switch (lang) {
    case kLangFrench: return 1;
    case kLangGerman: return 2;
    case kLangItalian: return 3;
    case kLangSpanish: return 4;
    case kLangEnglish:
    default: return 0;
  }
}

inline constexpr const char* LanguageNativeName(int lang) {
  switch (lang) {
    case kLangFrench: return "Français";
    case kLangGerman: return "Deutsch";
    case kLangItalian: return "Italiano";
    case kLangSpanish: return "Español";
    case kLangEnglish:
    default: return "English";
  }
}

namespace l10n {

struct Strings {
  const char* title_main;
  const char* title_wizard;
  const char* close_word;
  const char* back;

  const char* grp_file;
  const char* grp_display;
  const char* grp_performance;
  const char* grp_controls;
  const char* grp_language;
  const char* grp_about;

  const char* restart;
  const char* pause;
  const char* resume;
  const char* quit_game;

  const char* fullscreen;
  const char* off;
  const char* on;
  const char* resolution;
  const char* applying_res;
  const char* fov_default;
  const char* fov_prefix;
  const char* fov_mark_default;
  const char* fov_mark_rec;

  const char* frame_rate;
  const char* uncapped;
  const char* fps_note;
  const char* fps_counter;
  const char* vsync;
  const char* turbo_mode;

  const char* button_prompts;
  const char* prompt_auto;
  const char* prompt_keyboard;
  const char* configure_controls;
  const char* language_label;

  const char* version_prefix;
  const char* unknown;
  const char* made_by;
  const char* check_updates;
  const char* footer_hint;

  const char* restart_title;
  const char* quit_title;
  const char* lang_restart_title;
  const char* unsaved_lost;
  const char* lang_restart_body;
  const char* yes;
  const char* no;

  const char* wiz_keyboard;
  const char* wiz_press_input;
  const char* wiz_hint;
  const char* wiz_waiting;
  const char* wiz_deadzone_prefix;
  const char* wiz_defaults;
  const char* wiz_save;

  const char* toast_capture_kb;
  const char* toast_capture_gp;
  const char* toast_save_err;
  const char* toast_saved_controls;
  const char* toast_cleared;
  const char* toast_defaults;
  const char* toast_cancelled;
  const char* toast_updated;
  const char* toast_restart_launcher;
  const char* toast_restart_err;
  const char* toast_lang_saved_no_launcher;
  const char* toast_update_launcher;
  const char* toast_update_err;
  const char* toast_update_checking;
  const char* toast_res_pending;
  const char* toast_save_fullscreen_err;
  const char* toast_save_fov_err;
  const char* toast_save_fps_err;
  const char* toast_save_fps_counter_err;
  const char* toast_save_vsync_err;
  const char* toast_save_turbo_err;
  const char* toast_save_prompts_err;
  const char* toast_save_language_err;

  const char* bind_unbound;
  const char* bind_button_unknown;
  const char* bind_axis_unknown;
  const char* bind_dpad_up;
  const char* bind_dpad_down;
  const char* bind_dpad_left;
  const char* bind_dpad_right;
  const char* bind_ls_x;
  const char* bind_ls_y;
  const char* bind_rs_x;
  const char* bind_rs_y;
  const char* bind_space;
  const char* bind_key_prefix;

  std::array<const char*, dwg2::input::Count> actions;
};

inline constexpr Strings kTables[5] = {
    // 0: English (1)
    {
        "Recomp Menu",
        "Controls Configuration",
        "Close",
        "Back",
        "File",
        "Display",
        "Performance",
        "Controls",
        "Language",
        "About",
        "Restart",
        "Pause",
        "Resume",
        "Quit Game",
        "Fullscreen",
        "Off",
        "On",
        "Resolution",
        "Applying resolution...",
        "FOV: Default",
        "FOV: +",
        "Default",
        "+50% (Recommended)",
        "Frame Rate",
        "Uncapped",
        "Above 60 FPS is experimental. Game logic stays at 60 Hz.",
        "FPS Counter",
        "VSync",
        "Turbo Mode (+10%)",
        "Button Prompts",
        "Auto",
        "Keyboard",
        "Configure Controls",
        "Language",
        "Version: ",
        "Unknown",
        "Made by Adeel",
        "Check for Updates",
        "D-Pad / Arrows: Navigate     A / Enter: Select     B / Esc: Close",
        "Restart Game?",
        "Quit Game?",
        "Restart Required",
        "Unsaved progress will be lost.",
        "Restart now to apply the new language? Unsaved progress will be lost.",
        "Yes",
        "No",
        "Keyboard",
        "Press new input. Esc or B cancels.",
        "Bind: Click / Enter / A     Clear: Del / X     Scroll: Wheel / Arrows / D-Pad",
        "Waiting for input...",
        "Deadzone: ",
        "Restore Defaults",
        "Save & Apply",
        "Press a key or combo (Esc cancels).",
        "Press a button, trigger or stick (Esc/Back cancels).",
        "Could not save settings.ini.",
        "Mappings saved and applied.",
        "Binding cleared. Save to apply.",
        "Defaults restored. Save to apply.",
        "Binding cancelled.",
        "Binding updated. Save to apply.",
        "Start via DWG2R.exe to use Restart.",
        "Restart could not be requested.",
        "Language saved. Restart via DWG2R.exe to apply.",
        "Start via DWG2R.exe to check for updates.",
        "Update request could not be sent.",
        "Checking for updates...",
        "Resolution change pending.",
        "Could not save fullscreen setting.",
        "Could not save FOV setting.",
        "Could not save FPS setting.",
        "Could not save FPS counter.",
        "Could not save VSync setting.",
        "Could not save Turbo Mode.",
        "Could not save prompt style.",
        "Could not save language setting.",
        "Unbound",
        "Button ?",
        "Axis ?",
        "D-Pad Up",
        "D-Pad Down",
        "D-Pad Left",
        "D-Pad Right",
        "Left Stick X",
        "Left Stick Y",
        "Right Stick X",
        "Right Stick Y",
        "Space",
        "Key ",
        {
            "A / Cross",        "B / Circle",       "X / Square",       "Y / Triangle",
            "LB / L1",          "RB / R1",          "LT / L2",          "RT / R2",
            "Back / Create",    "Start / Options",  "Left stick click", "Right stick click",
            "D-pad Up",         "D-pad Down",       "D-pad Left",       "D-pad Right",
            "Left stick Up",    "Left stick Down",  "Left stick Left",  "Left stick Right",
            "Right stick Up",   "Right stick Down", "Right stick Left", "Right stick Right",
            "Fullscreen",       "Recomp Menu",
        },
    },
    // 1: French (4)
    {
        "Menu Recomp",
        "Configuration des contrôles",
        "Fermer",
        "Retour",
        "Fichier",
        "Affichage",
        "Performance",
        "Contrôles",
        "Langue",
        "À propos",
        "Redémarrer",
        "Pause",
        "Reprendre",
        "Quitter le jeu",
        "Plein écran",
        "Désact.",
        "Activé",
        "Résolution",
        "Application...",
        "Champ : Défaut",
        "Champ : +",
        "Défaut",
        "+50% (Recommandé)",
        "Fréquence d'images",
        "Illimité",
        "Au-delà de 60 FPS, expérimental. Logique de jeu à 60 Hz.",
        "Compteur FPS",
        "VSync",
        "Mode Turbo (+10%)",
        "Icônes de boutons",
        "Auto",
        "Clavier",
        "Configurer les contrôles",
        "Langue",
        "Version : ",
        "Inconnue",
        "Créé par Adeel",
        "Rechercher des mises à jour",
        "Croix / Flèches : Naviguer     A / Entrée : Valider     B / Échap : Fermer",
        "Redémarrer le jeu ?",
        "Quitter le jeu ?",
        "Redémarrage requis",
        "La progression non sauvegardée sera perdue.",
        "Redémarrer pour appliquer la langue ? Progrès non sauvé perdu.",
        "Oui",
        "Non",
        "Clavier",
        "Appuyez sur une touche. Échap ou B annule.",
        "Assigner : Clic / Entrée / A     Effacer : Suppr / X     Défiler : Molette / Flèches",
        "En attente...",
        "Zone morte : ",
        "Valeurs par défaut",
        "Enregistrer",
        "Appuyez sur une touche ou combinaison (Échap annule).",
        "Appuyez sur un bouton, gâchette ou stick (Échap/Back annule).",
        "Impossible d'enregistrer settings.ini.",
        "Touches enregistrées et appliquées.",
        "Touche effacée. Enregistrez pour appliquer.",
        "Défauts restaurés. Enregistrez pour appliquer.",
        "Assignation annulée.",
        "Touche mise à jour. Enregistrez pour appliquer.",
        "Lancez via DWG2R.exe pour redémarrer.",
        "Impossible de demander le redémarrage.",
        "Langue enregistrée. Relancez via DWG2R.exe.",
        "Lancez via DWG2R.exe pour les mises à jour.",
        "Impossible d'envoyer la demande de mise à jour.",
        "Recherche de mises à jour...",
        "Changement de résolution en cours.",
        "Impossible d'enregistrer le plein écran.",
        "Impossible d'enregistrer le champ de vision.",
        "Impossible d'enregistrer la fréquence FPS.",
        "Impossible d'enregistrer le compteur FPS.",
        "Impossible d'enregistrer la VSync.",
        "Impossible d'enregistrer le mode Turbo.",
        "Impossible d'enregistrer le style d'icônes.",
        "Impossible d'enregistrer la langue.",
        "Non assigné",
        "Bouton ?",
        "Axe ?",
        "Croix Haut",
        "Croix Bas",
        "Croix Gauche",
        "Croix Droite",
        "Stick gauche X",
        "Stick gauche Y",
        "Stick droit X",
        "Stick droit Y",
        "Espace",
        "Touche ",
        {
            "A / Croix",         "B / Cercle",        "X / Carré",         "Y / Triangle",
            "LB / L1",           "RB / R1",           "LT / L2",           "RT / R2",
            "Back / Create",     "Start / Options",   "Clic stick gauche", "Clic stick droit",
            "Croix Haut",        "Croix Bas",         "Croix Gauche",      "Croix Droite",
            "Stick gauche Haut", "Stick gauche Bas",  "Stick gauche Gauche","Stick gauche Droite",
            "Stick droit Haut",  "Stick droit Bas",   "Stick droit Gauche","Stick droit Droite",
            "Plein écran",       "Menu Recomp",
        },
    },
    // 2: German (3)
    {
        "Recomp-Menü",
        "Steuerungskonfiguration",
        "Schließen",
        "Zurück",
        "Datei",
        "Anzeige",
        "Leistung",
        "Steuerung",
        "Sprache",
        "Info",
        "Neustart",
        "Pause",
        "Fortsetzen",
        "Spiel beenden",
        "Vollbild",
        "Aus",
        "Ein",
        "Auflösung",
        "Wird angewendet...",
        "Sichtfeld: Std.",
        "Sichtfeld: +",
        "Standard",
        "+50% (Empfohlen)",
        "Bildrate",
        "Unbegrenzt",
        "Über 60 FPS ist experimentell. Spiellogik bleibt bei 60 Hz.",
        "FPS-Zähler",
        "VSync",
        "Turbo-Modus (+10%)",
        "Tastensymbole",
        "Auto",
        "Tastatur",
        "Steuerung anpassen",
        "Sprache",
        "Version: ",
        "Unbekannt",
        "Erstellt von Adeel",
        "Nach Updates suchen",
        "Steuerkreuz / Pfeile: Wählen     A / Enter: Bestätigen     B / Esc: Schließen",
        "Spiel neu starten?",
        "Spiel beenden?",
        "Neustart erforderlich",
        "Nicht gespeicherter Fortschritt geht verloren.",
        "Zum Übernehmen neu starten? Ungespeichertes geht verloren.",
        "Ja",
        "Nein",
        "Tastatur",
        "Neue Taste drücken. Esc oder B bricht ab.",
        "Belegen: Klick / Enter / A     Löschen: Entf / X     Scrollen: Rad / Pfeile",
        "Warte auf Eingabe...",
        "Totzone: ",
        "Standardwerte",
        "Speichern",
        "Taste oder Kombination drücken (Esc bricht ab).",
        "Taste, Trigger oder Stick drücken (Esc/Back bricht ab).",
        "settings.ini konnte nicht gespeichert werden.",
        "Belegung gespeichert und angewendet.",
        "Belegung gelöscht. Zum Übernehmen speichern.",
        "Standard wiederhergestellt. Zum Übernehmen speichern.",
        "Belegung abgebrochen.",
        "Belegung aktualisiert. Zum Übernehmen speichern.",
        "Für Neustart über DWG2R.exe starten.",
        "Neustart konnte nicht angefordert werden.",
        "Sprache gespeichert. Über DWG2R.exe neu starten.",
        "Für Updates über DWG2R.exe starten.",
        "Update-Anfrage fehlgeschlagen.",
        "Suche nach Updates...",
        "Auflösungswechsel läuft noch.",
        "Vollbild konnte nicht gespeichert werden.",
        "Sichtfeld konnte nicht gespeichert werden.",
        "FPS-Limit konnte nicht gespeichert werden.",
        "FPS-Zähler konnte nicht gespeichert werden.",
        "VSync konnte nicht gespeichert werden.",
        "Turbo-Modus konnte nicht gespeichert werden.",
        "Symbolstil konnte nicht gespeichert werden.",
        "Sprache konnte nicht gespeichert werden.",
        "Nicht belegt",
        "Taste ?",
        "Achse ?",
        "Steuerkreuz Hoch",
        "Steuerkreuz Runter",
        "Steuerkreuz Links",
        "Steuerkreuz Rechts",
        "Linker Stick X",
        "Linker Stick Y",
        "Rechter Stick X",
        "Rechter Stick Y",
        "Leertaste",
        "Taste ",
        {
            "A / Kreuz",          "B / Kreis",          "X / Viereck",        "Y / Dreieck",
            "LB / L1",            "RB / R1",            "LT / L2",            "RT / R2",
            "Back / Create",      "Start / Options",    "Linker Stick-Klick", "Rechter Stick-Klick",
            "Steuerkreuz Hoch",   "Steuerkreuz Runter", "Steuerkreuz Links",  "Steuerkreuz Rechts",
            "Linker Stick Hoch",  "Linker Stick Runter","Linker Stick Links", "Linker Stick Rechts",
            "Rechter Stick Hoch", "Rechter Stick Runter","Rechter Stick Links","Rechter Stick Rechts",
            "Vollbild",           "Recomp-Menü",
        },
    },
    // 3: Italian (6)
    {
        "Menu Recomp",
        "Configurazione comandi",
        "Chiudi",
        "Indietro",
        "File",
        "Schermo",
        "Prestazioni",
        "Comandi",
        "Lingua",
        "Info",
        "Riavvia",
        "Pausa",
        "Riprendi",
        "Esci dal gioco",
        "Schermo intero",
        "Disatt.",
        "Attivo",
        "Risoluzione",
        "Applicazione...",
        "FOV: Predef.",
        "FOV: +",
        "Predef.",
        "+50% (Consigliato)",
        "Frequenza fotogrammi",
        "Illimitato",
        "Oltre 60 FPS è sperimentale. La logica resta a 60 Hz.",
        "Contatore FPS",
        "VSync",
        "Modalità Turbo (+10%)",
        "Icone pulsanti",
        "Auto",
        "Tastiera",
        "Configura comandi",
        "Lingua",
        "Versione: ",
        "Sconosciuta",
        "Creato da Adeel",
        "Cerca aggiornamenti",
        "Croce / Frecce: Naviga     A / Invio: Seleziona     B / Esc: Chiudi",
        "Riavviare il gioco?",
        "Uscire dal gioco?",
        "Riavvio richiesto",
        "I progressi non salvati andranno persi.",
        "Riavviare per applicare la lingua? I dati non salvati si perdono.",
        "Sì",
        "No",
        "Tastiera",
        "Premi un comando. Esc o B annulla.",
        "Assegna: Clic / Invio / A     Cancella: Canc / X     Scorri: Rotella / Frecce",
        "In attesa...",
        "Zona morta: ",
        "Predefiniti",
        "Salva e applica",
        "Premi un tasto o combinazione (Esc annulla).",
        "Premi un pulsante, grilletto o levetta (Esc/Back annulla).",
        "Impossibile salvare settings.ini.",
        "Comandi salvati e applicati.",
        "Comando rimosso. Salva per applicare.",
        "Predefiniti ripristinati. Salva per applicare.",
        "Assegnazione annullata.",
        "Comando aggiornato. Salva per applicare.",
        "Avvia da DWG2R.exe per riavviare.",
        "Impossibile richiedere il riavvio.",
        "Lingua salvata. Riavvia da DWG2R.exe.",
        "Avvia da DWG2R.exe per gli aggiornamenti.",
        "Richiesta aggiornamento non inviata.",
        "Ricerca aggiornamenti...",
        "Cambio risoluzione in corso.",
        "Impossibile salvare schermo intero.",
        "Impossibile salvare il FOV.",
        "Impossibile salvare gli FPS.",
        "Impossibile salvare il contatore FPS.",
        "Impossibile salvare VSync.",
        "Impossibile salvare modalità Turbo.",
        "Impossibile salvare stile pulsanti.",
        "Impossibile salvare la lingua.",
        "Non assegnato",
        "Pulsante ?",
        "Asse ?",
        "Croce Su",
        "Croce Giù",
        "Croce Sinistra",
        "Croce Destra",
        "Levetta sin. X",
        "Levetta sin. Y",
        "Levetta des. X",
        "Levetta des. Y",
        "Spazio",
        "Tasto ",
        {
            "A / Croce",          "B / Cerchio",        "X / Quadrato",       "Y / Triangolo",
            "LB / L1",            "RB / R1",            "LT / L2",            "RT / R2",
            "Back / Create",      "Start / Options",    "Clic levetta sin.",  "Clic levetta des.",
            "Croce Su",           "Croce Giù",          "Croce Sinistra",     "Croce Destra",
            "Levetta sin. Su",    "Levetta sin. Giù",   "Levetta sin. SX",    "Levetta sin. DX",
            "Levetta des. Su",    "Levetta des. Giù",   "Levetta des. SX",    "Levetta des. DX",
            "Schermo intero",     "Menu Recomp",
        },
    },
    // 4: Spanish (5)
    {
        "Menú Recomp",
        "Configuración de controles",
        "Cerrar",
        "Volver",
        "Archivo",
        "Pantalla",
        "Rendimiento",
        "Controles",
        "Idioma",
        "Acerca de",
        "Reiniciar",
        "Pausa",
        "Reanudar",
        "Salir del juego",
        "Pantalla completa",
        "No",
        "Sí",
        "Resolución",
        "Aplicando...",
        "FOV: Predet.",
        "FOV: +",
        "Predet.",
        "+50% (Recomendado)",
        "Frecuencia de imagen",
        "Sin límite",
        "Más de 60 FPS es experimental. La lógica sigue a 60 Hz.",
        "Contador de FPS",
        "VSync",
        "Modo Turbo (+10%)",
        "Iconos de botones",
        "Auto",
        "Teclado",
        "Configurar controles",
        "Idioma",
        "Versión: ",
        "Desconocida",
        "Hecho por Adeel",
        "Buscar actualizaciones",
        "Cruceta / Flechas: Navegar     A / Enter: Elegir     B / Esc: Cerrar",
        "¿Reiniciar el juego?",
        "¿Salir del juego?",
        "Reinicio necesario",
        "Se perderá el progreso no guardado.",
        "¿Reiniciar para aplicar el idioma? Se perderá lo no guardado.",
        "Sí",
        "No",
        "Teclado",
        "Pulsa una entrada. Esc o B cancela.",
        "Asignar: Clic / Enter / A     Borrar: Supr / X     Desplazar: Rueda / Flechas",
        "Esperando entrada...",
        "Zona muerta: ",
        "Restaurar",
        "Guardar",
        "Pulsa una tecla o combinación (Esc cancela).",
        "Pulsa un botón, gatillo o stick (Esc/Back cancela).",
        "No se pudo guardar settings.ini.",
        "Controles guardados y aplicados.",
        "Asignación borrada. Guarda para aplicar.",
        "Valores restaurados. Guarda para aplicar.",
        "Asignación cancelada.",
        "Asignación actualizada. Guarda para aplicar.",
        "Inicia con DWG2R.exe para reiniciar.",
        "No se pudo solicitar el reinicio.",
        "Idioma guardado. Reinicia con DWG2R.exe.",
        "Inicia con DWG2R.exe para buscar actualizaciones.",
        "No se pudo enviar la solicitud.",
        "Buscando actualizaciones...",
        "Cambio de resolución pendiente.",
        "No se pudo guardar pantalla completa.",
        "No se pudo guardar el FOV.",
        "No se pudieron guardar los FPS.",
        "No se pudo guardar contador FPS.",
        "No se pudo guardar VSync.",
        "No se pudo guardar modo Turbo.",
        "No se pudo guardar estilo de iconos.",
        "No se pudo guardar el idioma.",
        "Sin asignar",
        "Botón ?",
        "Eje ?",
        "Cruceta Arriba",
        "Cruceta Abajo",
        "Cruceta Izquierda",
        "Cruceta Derecha",
        "Stick izq. X",
        "Stick izq. Y",
        "Stick der. X",
        "Stick der. Y",
        "Espacio",
        "Tecla ",
        {
            "A / Cruz",           "B / Círculo",        "X / Cuadrado",       "Y / Triángulo",
            "LB / L1",            "RB / R1",            "LT / L2",            "RT / R2",
            "Back / Create",      "Start / Options",    "Clic stick izq.",    "Clic stick der.",
            "Cruceta Arriba",     "Cruceta Abajo",      "Cruceta Izq.",       "Cruceta Der.",
            "Stick izq. Arriba",  "Stick izq. Abajo",   "Stick izq. Izq.",    "Stick izq. Der.",
            "Stick der. Arriba",  "Stick der. Abajo",   "Stick der. Izq.",    "Stick der. Der.",
            "Pantalla completa",  "Menú Recomp",
        },
    },
};

inline const Strings& Get(int lang) { return kTables[LanguageIndex(lang)]; }

inline std::string PromptStyleLabel(int lang, int style) {
  const auto& s = Get(lang);
  if (style == dwg2::prompts::kAuto) return s.prompt_auto;
  if (style == dwg2::prompts::kKeyboard) return s.prompt_keyboard;
  return dwg2::prompts::StyleName(style);
}

inline std::string FormatClose(int lang, int prompt_style) {
  const char* btn = dwg2::prompts::IsPlayStation(prompt_style)
                        ? "L2  "
                        : (prompt_style == dwg2::prompts::kKeyboard
                               ? "F1  "
                               : (prompt_style == dwg2::prompts::kSwitch ? "ZL  " : "LT  "));
  return std::string(btn) + Get(lang).close_word;
}

inline std::string FormatFov(int lang, int fov) {
  const auto& s = Get(lang);
  return fov == 0 ? std::string(s.fov_default) : std::string(s.fov_prefix) + std::to_string(fov) + "%";
}

inline std::string FormatDeadzone(int lang, int dz) {
  return std::string(Get(lang).wiz_deadzone_prefix) + std::to_string(dz) + "%";
}

inline std::string FormatVersion(int lang, std::string_view ver) {
  const auto& s = Get(lang);
  return std::string(s.version_prefix) + (ver.empty() ? std::string(s.unknown) : std::string(ver));
}

}  // namespace l10n
}  // namespace dwg2::recomp_menu
