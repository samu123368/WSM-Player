#include <ogc/conf.h>
#include <cstring>

#include "localization.h"
#include "../settings.h"

namespace
{
	const int LanguageCount = Localization::LanguageCount;
	const int MaxLocalizedCharacters = 72;

	// The System Menu font consumes UTF-16.  Keeping the source strings in
	// UTF-8 makes this table reviewable while the fixed buffers below keep every
	// returned pointer stable for the lifetime of the process.
	const char *const Text[ Localization::KeyCount ][ LanguageCount ] = {
		{
			u8"Wiiメニューに戻る?", u8"Back to Wii-Menu?", u8"Zurück zum Wii-Menü?",
			u8"Retour au menu Wii ?", u8"¿Volver al menú de Wii?", u8"Tornare al menu Wii?",
			u8"Terug naar Wii-menu?", u8"返回Wii菜单?", u8"返回Wii選單?", u8"Wii 메뉴로 돌아가기?"
		},
		{
			u8"WSM Player 設定", u8"WSM Player Settings", u8"WSM Player-Einstellungen",
			u8"Paramètres WSM Player", u8"Ajustes de WSM Player", u8"Impostazioni WSM Player",
			u8"WSM Player-instellingen", u8"WSM Player 设置", u8"WSM Player 設定", u8"WSM Player 설정"
		},
		{
			u8"HOMEボタンの動作", u8"HOME Button Action", u8"Aktion der HOME-Taste",
			u8"Action du bouton HOME", u8"Acción del botón HOME", u8"Azione tasto HOME",
			u8"Actie van HOME-knop", u8"HOME 键操作", u8"HOME 鍵操作", u8"HOME 버튼 동작"
		},
		{
			u8"Wiiメニューに戻る?", u8"Back to Wii-Menu?", u8"Zurück zum Wii-Menü?",
			u8"Retour au menu Wii ?", u8"¿Volver al menú de Wii?", u8"Tornare al menu Wii?",
			u8"Terug naar Wii-menu?", u8"返回Wii菜单?", u8"返回Wii選單?", u8"Wii 메뉴로 돌아가기?"
		},
		{
			u8"HOMEメニュー", u8"HOME Menu", u8"HOME-Menü", u8"Menu HOME",
			u8"Menú HOME", u8"Menu HOME", u8"HOME-menu", u8"HOME 菜单",
			u8"HOME 選單", u8"HOME 메뉴"
		},
		{
			u8"Homebrewアプリを隠す", u8"Hide Homebrew apps", u8"Homebrew-Apps ausblenden",
			u8"Masquer les apps Homebrew", u8"Ocultar apps Homebrew", u8"Nascondi app Homebrew",
			u8"Homebrew-apps verbergen", u8"隐藏 Homebrew 应用", u8"隱藏 Homebrew 應用程式",
			u8"Homebrew 앱 숨기기"
		},
		{
			u8"STARTで選択項目を起動", u8"Start launches title/app", u8"Start startet Titel/App",
			u8"Start lance la chaîne/app", u8"Start inicia canal/app", u8"Start avvia canale/app",
			u8"Start opent kanaal/app", u8"START 启动频道/应用", u8"START 啟動頻道/應用程式",
			u8"START로 채널/앱 실행"
		},
		{
			u8"はい", u8"Yes", u8"Ja", u8"Oui", u8"Sí", u8"Sì", u8"Ja", u8"是", u8"是", u8"예"
		},
		{
			u8"いいえ", u8"No", u8"Nein", u8"Non", u8"No", u8"No",
			u8"Nee", u8"否", u8"否", u8"아니요"
		},
		{
			u8"音楽", u8"Music", u8"Musik", u8"Musique", u8"Música", u8"Musica",
			u8"Muziek", u8"音乐", u8"音樂", u8"음악"
		},
		{
			u8"戻る", u8"Back", u8"Zurück", u8"Retour", u8"Volver", u8"Indietro",
			u8"Terug", u8"返回", u8"返回", u8"돌아가기"
		},
		{
			u8"設定を保存できません", u8"Could not save settings", u8"Einstellungen nicht gespeichert",
			u8"Échec de l'enregistrement", u8"No se pudieron guardar los ajustes",
			u8"Impossibile salvare le impostazioni", u8"Instellingen opslaan mislukt",
			u8"无法保存设置", u8"無法儲存設定", u8"설정을 저장하지 못했습니다"
		},
		{
			u8"マリオカートチャンネル", u8"Mario Kart Channel", u8"Mario Kart-Kanal",
			u8"Chaîne Mario Kart", u8"Canal Mario Kart", u8"Canale Mario Kart",
			u8"Mario Kart-kanaal", u8"马力欧卡丁车频道", u8"瑪利歐賽車頻道", u8"마리오 카트 채널"
		}
	};

	struct Utf8Translation
	{
		const char *english;
		const char *text[ LanguageCount ];
	};

#define T10(en,ja,de,fr,es,it,nl,zh,zt,ko) \
	{ en, { ja, en, de, fr, es, it, nl, zh, zt, ko } }
	const Utf8Translation Utf8Text[] = {
#include "localization_updates.inc"
		T10("EULA channel resources are not installed.",u8"利用規約チャンネルのデータがありません。",u8"Die Ressourcen des Vertragskanals fehlen.",u8"Les ressources de la chaîne du contrat sont absentes.",u8"Faltan los recursos del canal del contrato.",u8"Mancano le risorse del canale del contratto.",u8"De bronnen van het contractkanaal ontbreken.","EULA channel resources are not installed.","EULA channel resources are not installed.","EULA channel resources are not installed."),
		T10("The historical agreement was provided online.",u8"以前の利用規約はオンラインで提供されました。",u8"Der frühere Vertrag wurde online bereitgestellt.",u8"L'ancien contrat était fourni en ligne.",u8"El contrato original se ofrecía en línea.",u8"Il contratto originale era disponibile online.",u8"De oorspronkelijke overeenkomst stond online.","The historical agreement was provided online.","The historical agreement was provided online.","The historical agreement was provided online."),
		T10("Its text is not stored in NAND.",u8"その本文はNANDに保存されていません。",u8"Sein Text ist nicht im NAND gespeichert.",u8"Son texte n'est pas enregistré dans la NAND.",u8"Su texto no está guardado en la NAND.",u8"Il testo non è salvato nella NAND.",u8"De tekst is niet opgeslagen in NAND.","Its text is not stored in NAND.","Its text is not stored in NAND.","Its text is not stored in NAND."),
		T10("Connect a Nunchuk to fly",u8"ヌンチャクを接続してください",u8"Zum Fliegen einen Nunchuk anschließen",u8"Connectez un Nunchuk pour voler",u8"Conecta un Nunchuk para volar",u8"Collega un Nunchuk per volare",u8"Sluit een Nunchuk aan om te vliegen","Connect a Nunchuk to fly","Connect a Nunchuk to fly","Connect a Nunchuk to fly"),
		T10("Stick: Move   D-pad: Look   C/Z: Up/Down",u8"スティック: 移動   十字キー: 視点   C/Z: 上/下",u8"Stick: Bewegen   Steuerkreuz: Blick   C/Z: Hoch/Runter",u8"Stick: Déplacer   Croix: Regarder   C/Z: Monter/Descendre",u8"Palanca: Mover   Cruceta: Mirar   C/Z: Subir/Bajar",u8"Stick: Muovi   Croce: Guarda   C/Z: Su/Giù",u8"Stick: Bewegen   D-pad: Kijken   C/Z: Omhoog/Omlaag","Stick: Move   D-pad: Look   C/Z: Up/Down","Stick: Move   D-pad: Look   C/Z: Up/Down","Stick: Move   D-pad: Look   C/Z: Up/Down"),
		T10("+/-: Speed   A: Reset   B: Exit",u8"+/-: 速度   A: リセット   B: 終了",u8"+/-: Tempo   A: Zurücksetzen   B: Ende",u8"+/-: Vitesse   A: Réinitialiser   B: Quitter",u8"+/-: Velocidad   A: Restablecer   B: Salir",u8"+/-: Velocità   A: Ripristina   B: Esci",u8"+/-: Snelheid   A: Herstellen   B: Afsluiten","+/-: Speed   A: Reset   B: Exit","+/-: Speed   A: Reset   B: Exit","+/-: Speed   A: Reset   B: Exit"),
		T10("3D: On",u8"3D: オン",u8"3D: Ein",u8"3D: Oui",u8"3D: Sí",u8"3D: Sì",u8"3D: Aan","3D: On","3D: On","3D: On"),
		T10("3D: Off",u8"3D: オフ",u8"3D: Aus",u8"3D: Non",u8"3D: No",u8"3D: No",u8"3D: Uit","3D: Off","3D: Off","3D: Off"),
		T10("Layers: On",u8"レイヤー: オン",u8"Ebenen: Ein",u8"Couches: Oui",u8"Capas: Sí",u8"Livelli: Sì",u8"Lagen: Aan","Layers: On","Layers: On","Layers: On"),
		T10("Layers: Off",u8"レイヤー: オフ",u8"Ebenen: Aus",u8"Couches: Non",u8"Capas: No",u8"Livelli: No",u8"Lagen: Uit","Layers: Off","Layers: Off","Layers: Off"),
		T10("D-pad: Orbit   +/-: Zoom   A: Reset   B: Exit",u8"十字キー: 回転   +/-: ズーム   A: リセット   B: 終了",u8"Steuerkreuz: Drehen   +/-: Zoom   A: Reset   B: Ende",u8"Croix: Tourner   +/-: Zoom   A: Réinitialiser   B: Quitter",u8"Cruceta: Girar   +/-: Zoom   A: Restablecer   B: Salir",u8"Croce: Ruota   +/-: Zoom   A: Ripristina   B: Esci",u8"D-pad: Draaien   +/-: Zoom   A: Herstellen   B: Afsluiten","D-pad: Orbit   +/-: Zoom   A: Reset   B: Exit","D-pad: Orbit   +/-: Zoom   A: Reset   B: Exit","D-pad: Orbit   +/-: Zoom   A: Reset   B: Exit"),
		T10(u8"Pick on screen",u8"画面で選択",u8"Auf Bildschirm wählen",u8"Choisir à l’écran",u8"Elegir en pantalla",u8"Scegli sullo schermo",u8"Op scherm kiezen",u8"Pick on screen",u8"Pick on screen",u8"Pick on screen"),
		T10(u8"Interact",u8"操作",u8"Bedienen",u8"Interagir",u8"Interactuar",u8"Interagisci",u8"Bedienen",u8"Interact",u8"Interact",u8"Interact"),
		T10(u8"Click an element",u8"要素を選択",u8"Element anklicken",u8"Cliquez sur un élément",u8"Pulsa un elemento",u8"Seleziona un elemento",u8"Klik op een element",u8"Click an element",u8"Click an element",u8"Click an element"),
		T10(u8"Show",u8"表示",u8"Einblenden",u8"Afficher",u8"Mostrar",u8"Mostra",u8"Tonen",u8"Show",u8"Show",u8"Show"),
		T10(u8"B: Elements",u8"B: 要素",u8"B: Elemente",u8"B : Éléments",u8"B: Elementos",u8"B: Elementi",u8"B: Elementen",u8"B: Elements",u8"B: Elements",u8"B: Elements"),
		T10(u8"Elements",u8"要素",u8"Elemente",u8"Éléments",u8"Elementos",u8"Elementi",u8"Elementen",u8"Elements",u8"Elements",u8"Elements"),
		T10(u8"Container",u8"グループ",u8"Gruppe",u8"Groupe",u8"Grupo",u8"Gruppo",u8"Groep",u8"Container",u8"Container",u8"Container"),
		T10(u8"Image",u8"画像",u8"Bild",u8"Image",u8"Imagen",u8"Immagine",u8"Afbeelding",u8"Image",u8"Image",u8"Image"),
		T10(u8"Text",u8"テキスト",u8"Text",u8"Texte",u8"Texto",u8"Testo",u8"Tekst",u8"Text",u8"Text",u8"Text"),
		T10(u8"Button",u8"ボタン",u8"Schaltfläche",u8"Bouton",u8"Botón",u8"Pulsante",u8"Knop",u8"Button",u8"Button",u8"Button"),
		T10(u8"Panel",u8"パネル",u8"Fläche",u8"Panneau",u8"Panel",u8"Pannello",u8"Paneel",u8"Panel",u8"Panel",u8"Panel"),
		T10(u8"Channel",u8"チャンネル",u8"Kanal",u8"Chaîne",u8"Canal",u8"Canale",u8"Kanaal",u8"Channel",u8"Channel",u8"Channel"),
		T10(u8"Mask",u8"マスク",u8"Maske",u8"Masque",u8"Máscara",u8"Maschera",u8"Masker",u8"Mask",u8"Mask",u8"Mask"),
		T10(u8"Hide",u8"非表示",u8"Ausblenden",u8"Masquer",u8"Ocultar",u8"Nascondi",u8"Verbergen",u8"Hide",u8"Hide",u8"Hide"),
		T10(u8"Masks and clipping",u8"マスクとクリッピング",u8"Masken und Zuschnitt",u8"Masques et découpage",u8"Máscaras y recorte",u8"Maschere e ritaglio",u8"Maskers en begrenzing",u8"Masks and clipping",u8"Masks and clipping",u8"Masks and clipping"),
		T10(u8"Restore all elements",u8"すべて元に戻す",u8"Alle Elemente zurücksetzen",u8"Rétablir tous les éléments",u8"Restaurar todos los elementos",u8"Ripristina tutti gli elementi",u8"Alle elementen herstellen",u8"Restore all elements",u8"Restore all elements",u8"Restore all elements"),
		T10(u8"Hidden",u8"非表示",u8"Ausgeblendet",u8"Masqué",u8"Oculto",u8"Nascosto",u8"Verborgen",u8"Hidden",u8"Hidden",u8"Hidden"),
		T10(u8"Shown",u8"表示",u8"Eingeblendet",u8"Affiché",u8"Visible",u8"Visibile",u8"Zichtbaar",u8"Shown",u8"Shown",u8"Shown"),
		T10(u8"A: Change   B: Back   1: Masks",u8"A: 変更   B: 戻る   1: マスク",u8"A: Ändern   B: Zurück   1: Masken",u8"A : Modifier   B : Retour   1 : Masques",u8"A: Cambiar   B: Volver   1: Máscaras",u8"A: Cambia   B: Indietro   1: Maschere",u8"A: Wijzigen   B: Terug   1: Maskers",u8"A: Change   B: Back   1: Masks",u8"A: Change   B: Back   1: Masks",u8"A: Change   B: Back   1: Masks"),
		T10(u8"2: Elements   1: Masks",u8"2: 要素   1: マスク",u8"2: Elemente   1: Masken",u8"2 : Éléments   1 : Masques",u8"2: Elementos   1: Máscaras",u8"2: Elementi   1: Maschere",u8"2: Elementen   1: Maskers",u8"2: Elements   1: Masks",u8"2: Elements   1: Masks",u8"2: Elements   1: Masks"),
		T10("Free camera",u8"フリーカメラ",u8"Freie Kamera",u8"Caméra libre",u8"Cámara libre",u8"Telecamera libera",u8"Vrije camera",u8"自由镜头",u8"自由鏡頭",u8"자유 카메라"),
		T10("D-pad: Pan   +/-: Zoom   A: Reset   B: Exit",u8"十字キー: 移動   +/-: ズーム   A: リセット   B: 終了",u8"Steuerkreuz: Bewegen   +/-: Zoom   A: Reset   B: Ende",u8"Croix: Déplacer   +/-: Zoom   A: Réinitialiser   B: Quitter",u8"Cruceta: Mover   +/-: Zoom   A: Restablecer   B: Salir",u8"Croce: Muovi   +/-: Zoom   A: Ripristina   B: Esci",u8"D-pad: Bewegen   +/-: Zoom   A: Herstellen   B: Afsluiten",u8"方向键: 移动   +/-: 缩放   A: 重置   B: 退出",u8"方向鍵: 移動   +/-: 縮放   A: 重設   B: 離開",u8"방향키: 이동   +/-: 확대   A: 초기화   B: 종료"),
		T10("Audio Settings",u8"オーディオ設定",u8"Audioeinstellungen",u8"Paramètres audio",u8"Ajustes de audio",u8"Impostazioni audio",u8"Audio-instellingen",u8"音频设置",u8"音訊設定",u8"오디오 설정"),
		T10("Controller & USB",u8"コントローラとUSB",u8"Controller & USB",u8"Manettes et USB",u8"Mandos y USB",u8"Controller e USB",u8"Controllers en USB",u8"控制器与 USB",u8"控制器與 USB",u8"컨트롤러 및 USB"),
		T10("USB HID input",u8"USB HID入力",u8"USB-HID-Eingabe",u8"Entrée USB HID",u8"Entrada USB HID",u8"Ingresso USB HID",u8"USB-HID-invoer",u8"USB HID 输入",u8"USB HID 輸入",u8"USB HID 입력"),
		T10("Mouse pointer speed",u8"マウスポインター速度",u8"Mauszeigergeschwindigkeit",u8"Vitesse du pointeur",u8"Velocidad del puntero",u8"Velocità puntatore",u8"Snelheid muisaanwijzer",u8"鼠标指针速度",u8"滑鼠游標速度",u8"마우스 포인터 속도"),
		T10("Clock format",u8"時計表示",u8"Uhrzeitformat",u8"Format de l’heure",u8"Formato de reloj",u8"Formato orologio",u8"Tijdnotatie",u8"时钟格式",u8"時鐘格式",u8"시계 형식"),
		T10("12-hour",u8"12時間表示",u8"12 Stunden",u8"12 heures",u8"12 horas",u8"12 ore",u8"12 uur",u8"12 小时",u8"12 小時",u8"12시간"),
		T10("24-hour",u8"24時間表示",u8"24 Stunden",u8"24 heures",u8"24 horas",u8"24 ore",u8"24 uur",u8"24 小时",u8"24 小時",u8"24시간"),
		T10("Themes",u8"テーマ",u8"Designs",u8"Thèmes",u8"Temas",u8"Temi",u8"Thema's",u8"主题",u8"主題",u8"테마"),
		T10("Original Wii Menu",u8"オリジナルWiiメニュー",u8"Originales Wii-Menü",u8"Menu Wii d’origine",u8"Menú de Wii original",u8"Menu Wii originale",u8"Origineel Wii-menu",u8"原版 Wii 菜单",u8"原版 Wii 選單",u8"원본 Wii 메뉴"),
		T10("Active after restart",u8"再起動後に有効",u8"Nach Neustart aktiv",u8"Actif après redémarrage",u8"Activo tras reiniciar",u8"Attivo dopo il riavvio",u8"Actief na herstart",u8"重启后启用",u8"重新啟動後啟用",u8"재시작 후 적용"),
		T10("MYM needs compilation",u8"MYMは変換が必要",u8"MYM muss kompiliert werden",u8"MYM doit être compilé",u8"MYM necesita compilarse",u8"MYM deve essere compilato",u8"MYM moet worden gecompileerd",u8"MYM 需要编译",u8"MYM 需要編譯",u8"MYM 컴파일 필요"),
		T10("Channel Settings",u8"チャンネル設定",u8"Kanaleinstellungen",u8"Paramètres des chaînes",u8"Ajustes de canales",u8"Impostazioni canali",u8"Kanaalinstellingen",u8"频道设置",u8"頻道設定",u8"채널 설정"),
		T10("Refresh from Wii Menu",u8"Wiiメニューから更新",u8"Vom Wii-Menü aktualisieren",u8"Actualiser depuis le menu Wii",u8"Actualizar desde el menú de Wii",u8"Aggiorna dal menu Wii",u8"Vernieuwen vanuit het Wii-menu",u8"从 Wii 菜单刷新",u8"從 Wii 選單更新",u8"Wii 메뉴에서 새로 고침"),
		T10("News Channel",u8"ニュースチャンネル",u8"Nachrichtenkanal",u8"Chaîne infos",u8"Canal Noticias",u8"Canale Notizie",u8"Nieuwskanaal",u8"新闻频道",u8"新聞頻道",u8"뉴스 채널"),
		T10("Forecast Channel",u8"お天気チャンネル",u8"Wetterkanal",u8"Chaîne météo",u8"Canal Tiempo",u8"Canale Meteo",u8"Weerkanaal",u8"天气频道",u8"天氣頻道",u8"날씨 채널"),
		T10("Nintendo Channel",u8"みんなのニンテンドーチャンネル",u8"Nintendo-Kanal",u8"Chaîne Nintendo",u8"Canal Nintendo",u8"Canale Nintendo",u8"Nintendo-kanaal",u8"任天堂频道",u8"任天堂頻道",u8"닌텐도 채널"),
		T10("Photo Channel",u8"写真チャンネル",u8"Fotokanal",u8"Chaîne photos",u8"Canal Fotos",u8"Canale Foto",u8"Fotokanaal",u8"照片频道",u8"相片頻道",u8"사진 채널"),
		T10("Everybody Votes / Meinungs Kanal",u8"みんなで投票チャンネル",u8"Meinungskanal",u8"Chaîne votes",u8"Canal Opiniones",u8"Canale Vota Anche Tu",u8"Iedereen stemt",u8"大家投票频道",u8"大家投票頻道",u8"모두의 투표 채널"),
		T10("Menu music",u8"メニュー音楽",u8"Menümusik",u8"Musique du menu",u8"Música del menú",u8"Musica del menu",u8"Muziek in menu",u8"菜单音乐",u8"選單音樂",u8"메뉴 음악"),
		T10("Use custom headlines",u8"カスタム見出しを使う",u8"Eigene Schlagzeilen",u8"Utiliser les titres personnalisés",u8"Usar titulares personalizados",u8"Usa titoli personalizzati",u8"Eigen koppen gebruiken",u8"使用自定义新闻标题",u8"使用自訂新聞標題",u8"사용자 헤드라인 사용"),
		T10("Use custom forecast",u8"カスタム予報を使う",u8"Eigene Vorhersage",u8"Utiliser les prévisions personnalisées",u8"Usar pronóstico personalizado",u8"Usa previsioni personalizzate",u8"Eigen voorspelling gebruiken",u8"使用自定义天气预报",u8"使用自訂天氣預報",u8"사용자 예보 사용"),
		T10("Article",u8"記事",u8"Artikel",u8"Article",u8"Artículo",u8"Articolo",u8"Artikel",u8"文章",u8"文章",u8"기사"),
		T10("Edit text",u8"文章を編集",u8"Text bearbeiten",u8"Modifier le texte",u8"Editar texto",u8"Modifica testo",u8"Tekst bewerken",u8"编辑文本",u8"編輯文字",u8"텍스트 편집"),
		T10("Add article",u8"記事を追加",u8"Artikel hinzufügen",u8"Ajouter un article",u8"Añadir artículo",u8"Aggiungi articolo",u8"Artikel toevoegen",u8"添加文章",u8"新增文章",u8"기사 추가"),
		T10("Delete article",u8"記事を削除",u8"Artikel löschen",u8"Supprimer l’article",u8"Borrar artículo",u8"Elimina articolo",u8"Artikel verwijderen",u8"删除文章",u8"刪除文章",u8"기사 삭제"),
		T10("Move article",u8"記事を移動",u8"Artikel verschieben",u8"Déplacer l’article",u8"Mover artículo",u8"Sposta articolo",u8"Artikel verplaatsen",u8"移动文章",u8"移動文章",u8"기사 이동"),
		T10("City",u8"都市",u8"Stadt",u8"Ville",u8"Ciudad",u8"Città",u8"Stad",u8"城市",u8"城市",u8"도시"),
		T10("Temperature",u8"気温",u8"Temperatur",u8"Température",u8"Temperatura",u8"Temperatura",u8"Temperatuur",u8"温度",u8"溫度",u8"온도"),
		T10("Unit",u8"単位",u8"Einheit",u8"Unité",u8"Unidad",u8"Unità",u8"Eenheid",u8"单位",u8"單位",u8"단위"),
		T10("Weather name",u8"天気の名前",u8"Wettername",u8"Nom de la météo",u8"Nombre del tiempo",u8"Nome meteo",u8"Weernaam",u8"天气名称",u8"天氣名稱",u8"날씨 이름"),
		T10("Weather icon",u8"天気アイコン",u8"Wettersymbol",u8"Icône météo",u8"Icono del tiempo",u8"Icona meteo",u8"Weerpictogram",u8"天气图标",u8"天氣圖示",u8"날씨 아이콘"),
		T10("Japanese UI",u8"日本版UI",u8"Japanische UI",u8"Interface japonaise",u8"Interfaz japonesa",u8"Interfaccia giapponese",u8"Japanse interface",u8"日版界面",u8"日版介面",u8"일본어 UI"),
		T10("Custom icon story",u8"カスタムアイコン記事",u8"Eigene Icon-Story",u8"Article d’icône perso",u8"Historia de icono",u8"Storia icona",u8"Eigen pictogramverhaal",u8"自定义图标故事",u8"自訂圖示故事",u8"사용자 아이콘 이야기"),
		T10("Icon message",u8"アイコンメッセージ",u8"Icon-Nachricht",u8"Message de l’icône",u8"Mensaje del icono",u8"Messaggio icona",u8"Pictogramtekst",u8"图标消息",u8"圖示訊息",u8"아이콘 메시지"),
		T10("Icon image",u8"アイコン画像",u8"Icon-Bild",u8"Image de l’icône",u8"Imagen del icono",u8"Immagine icona",u8"Pictogramafbeelding",u8"图标图片",u8"圖示圖片",u8"아이콘 이미지"),
		T10("Image sizing",u8"画像サイズ",u8"Bildanpassung",u8"Taille de l’image",u8"Ajuste de imagen",u8"Dimensione immagine",u8"Afbeeldingsgrootte",u8"图片缩放",u8"圖片縮放",u8"이미지 크기"),
		T10("Show selected photo",u8"選択した写真を表示",u8"Gewähltes Foto zeigen",u8"Afficher la photo",u8"Mostrar foto elegida",u8"Mostra foto scelta",u8"Gekozen foto tonen",u8"显示所选照片",u8"顯示所選相片",u8"선택한 사진 표시"),
		T10("Banner photo",u8"バナー写真",u8"Bannerfoto",u8"Photo de bannière",u8"Foto del banner",u8"Foto banner",u8"Bannerfoto",u8"横幅照片",u8"橫幅相片",u8"배너 사진"),
		T10("Preset / mode",u8"プリセット / モード",u8"Vorgabe / Modus",u8"Préréglage / mode",u8"Preajuste / modo",u8"Predefinito / modalità",u8"Voorinstelling / modus",u8"预设 / 模式",u8"預設 / 模式",u8"프리셋 / 모드"),
		T10("First blue",u8"最初の青",u8"Erste blaue Blase",u8"Première bulle bleue",u8"Primera azul",u8"Prima blu",u8"Eerste blauwe",u8"第一个蓝色气泡",u8"第一個藍色泡泡",u8"첫 파란 말풍선"),
		T10("Green question",u8"緑の質問",u8"Grüne Frage",u8"Question verte",u8"Pregunta verde",u8"Domanda verde",u8"Groene vraag",u8"绿色问题",u8"綠色問題",u8"초록 질문"),
		T10("Final blue",u8"最後の青",u8"Letzte blaue Blase",u8"Dernière bulle bleue",u8"Azul final",u8"Blu finale",u8"Laatste blauwe",u8"最后蓝色气泡",u8"最後藍色泡泡",u8"마지막 파란 말풍선"),
		T10("Console Nickname",u8"本体ニックネーム",u8"Konsolen-Spitzname",u8"Surnom de la console",u8"Apodo de la consola",u8"Nickname console",u8"Naam van het Wii-systeem",u8"主机昵称",u8"主機暱稱",u8"본체 닉네임"),
		T10("Calendar",u8"カレンダー",u8"Kalender",u8"Calendrier",u8"Calendario",u8"Calendario",u8"Kalender",u8"日历",u8"日曆",u8"달력"),
		T10("Screen",u8"画面",u8"Bildschirm",u8"Écran",u8"Pantalla",u8"Schermo",u8"Beeldscherm",u8"屏幕",u8"畫面",u8"화면"),
		T10("Sound",u8"サウンド",u8"Sound",u8"Son",u8"Sonido",u8"Audio",u8"Geluid",u8"声音",u8"聲音",u8"사운드"),
		T10("Parental Controls",u8"ペアレンタルコントロール",u8"Altersbeschränkungen",u8"Contrôle parental",u8"Control parental",u8"Filtro famiglia",u8"Ouderlijk toezicht",u8"家长控制",u8"家長控制",u8"청소년 보호 기능"),
		T10("Sensor Bar",u8"センサーバー",u8"Sensorleiste",u8"Capteur",u8"Barra de sensores",u8"Barra sensore",u8"Sensorbalk",u8"感应条",u8"感應器",u8"센서 바"),
		T10("Internet",u8"インターネット",u8"Internet",u8"Internet",u8"Internet",u8"Internet",u8"Internet",u8"互联网",u8"網際網路",u8"인터넷"),
		T10("Language",u8"言語",u8"Sprache",u8"Langue",u8"Idioma",u8"Lingua",u8"Taal",u8"语言",u8"語言",u8"언어"),
		T10("Country",u8"国",u8"Land",u8"Pays",u8"País",u8"Paese",u8"Land",u8"国家",u8"國家",u8"국가"),
		T10("Wii System Update",u8"Wii本体の更新",u8"Wii-System-Update",u8"Mise à jour Wii",u8"Actualización de Wii",u8"Aggiornamento Wii",u8"Wii-systeemupdate",u8"Wii 系统更新",u8"Wii 主機更新",u8"Wii 본체 업데이트"),
		T10("Performing a Wii system update.",u8"Wii本体を更新しています。",u8"Das Wii-System wird aktualisiert.",u8"Mise à jour de la Wii en cours.",u8"Actualizando la consola Wii.",u8"Aggiornamento della console Wii.",u8"Het Wii-systeem wordt bijgewerkt.",u8"正在更新 Wii 主机。",u8"正在更新 Wii 主機。",u8"Wii 본체를 업데이트하고 있습니다."),
		T10("Please do not turn off the power.",u8"電源を切らないでください。",u8"Bitte schalte die Konsole nicht aus.",u8"N'éteignez pas la console.",u8"No apagues la consola.",u8"Non spegnere la console.",u8"Schakel het systeem niet uit.",u8"请勿关闭电源。",u8"請勿關閉電源。",u8"전원을 끄지 마세요."),
		T10("The system update was completed successfully.",u8"本体の更新が完了しました。",u8"Das System-Update wurde erfolgreich abgeschlossen.",u8"La mise à jour a réussi.",u8"La actualización se ha completado correctamente.",u8"Aggiornamento completato correttamente.",u8"De systeemupdate is voltooid.",u8"系统更新已成功完成。",u8"系統更新已成功完成。",u8"시스템 업데이트가 완료되었습니다."),
		T10("Press A to continue.",u8"Aボタンを押してください。",u8"Drücke A, um fortzufahren.",u8"Appuyez sur A pour continuer.",u8"Pulsa A para continuar.",u8"Premi A per continuare.",u8"Druk op A om verder te gaan.",u8"按 A 继续。",u8"按 A 繼續。",u8"계속하려면 A를 누르세요."),
		T10("Format Wii System Memory",u8"Wii本体の初期化",u8"Wii-Speicher formatieren",u8"Formater la mémoire Wii",u8"Formatear memoria Wii",u8"Formatta memoria Wii",u8"Wii-geheugen wissen",u8"格式化 Wii 主机内存",u8"格式化 Wii 主機記憶體",u8"Wii 본체 메모리 포맷"),
		T10("Read-only",u8"閲覧のみ",u8"Nur ansehen",u8"Lecture seule",u8"Solo lectura",u8"Sola lettura",u8"Alleen-lezen",u8"只读",u8"唯讀",u8"읽기 전용"),
		T10("WSM Player Settings",u8"WSM Player 設定",u8"WSM Player-Einstellungen",u8"Paramètres WSM Player",u8"Ajustes de WSM Player",u8"Impostazioni WSM Player",u8"WSM Player-instellingen",u8"WSM Player 设置",u8"WSM Player 設定",u8"WSM Player 설정"),
		T10("On",u8"オン",u8"Ein",u8"Oui",u8"Sí",u8"Sì",u8"Aan",u8"开",u8"開",u8"켜기"),
		T10("Off",u8"オフ",u8"Aus",u8"Non",u8"No",u8"No",u8"Uit",u8"关",u8"關",u8"끄기"),
		T10("Fill",u8"全体に表示",u8"Ausfüllen",u8"Remplir",u8"Rellenar",u8"Riempi",u8"Vullen",u8"填充",u8"填滿",u8"채우기"),
		T10("Fit",u8"全体を収める",u8"Einpassen",u8"Ajuster",u8"Ajustar",u8"Adatta",u8"Passend",u8"适合",u8"符合",u8"맞추기"),
		T10("Press A",u8"Aを押す",u8"A drücken",u8"Appuyer sur A",u8"Pulsa A",u8"Premi A",u8"Druk op A",u8"按 A",u8"按 A",u8"A 누르기"),
		T10("Left / Right",u8"左 / 右",u8"Links / Rechts",u8"Gauche / Droite",u8"Izquierda / Derecha",u8"Sinistra / Destra",u8"Links / Rechts",u8"左 / 右",u8"左 / 右",u8"왼쪽 / 오른쪽")
		,T10("On (refresh at startup)",u8"オン（起動時に更新）",u8"Ein (beim Start aktualisieren)",u8"Oui (actualiser au démarrage)",u8"Sí (actualizar al iniciar)",u8"Sì (aggiorna all’avvio)",u8"Aan (bij opstart vernieuwen)",u8"开（启动时刷新）",u8"開（啟動時更新）",u8"켜기 (시작 시 새로고침)")
		,T10("Off (built-in)",u8"オフ（内蔵データ）",u8"Aus (integriert)",u8"Non (intégré)",u8"No (integrado)",u8"No (integrato)",u8"Uit (ingebouwd)",u8"关（内置）",u8"關（內建）",u8"끄기 (내장)"),
		T10("Fahrenheit (F)",u8"華氏 (F)",u8"Fahrenheit (F)",u8"Fahrenheit (F)",u8"Fahrenheit (F)",u8"Fahrenheit (F)",u8"Fahrenheit (F)",u8"华氏 (F)",u8"華氏 (F)",u8"화씨 (F)"),
		T10("Celsius (C)",u8"摂氏 (C)",u8"Celsius (C)",u8"Celsius (C)",u8"Celsius (C)",u8"Celsius (C)",u8"Celsius (C)",u8"摄氏 (C)",u8"攝氏 (C)",u8"섭씨 (C)"),
		T10("On (HAFJ UI + icons)",u8"オン（日本版UI＋アイコン）",u8"Ein (HAFJ-UI + Symbole)",u8"Oui (interface HAFJ + icônes)",u8"Sí (interfaz HAFJ + iconos)",u8"Sì (UI HAFJ + icone)",u8"Aan (HAFJ-interface + pictogrammen)",u8"开（日版界面和图标）",u8"開（日版介面和圖示）",u8"켜기 (HAFJ UI + 아이콘)"),
		T10("Off (worldwide UI)",u8"オフ（海外版UI）",u8"Aus (weltweite UI)",u8"Non (interface mondiale)",u8"No (interfaz mundial)",u8"No (UI internazionale)",u8"Uit (wereldwijde interface)",u8"关（国际版界面）",u8"關（國際版介面）",u8"끄기 (국제 UI)"),
		T10("Off (original)",u8"オフ（オリジナル）",u8"Aus (Original)",u8"Non (original)",u8"No (original)",u8"No (originale)",u8"Uit (origineel)",u8"关（原版）",u8"關（原版）",u8"끄기 (원본)"),
		T10("Choose from SD...",u8"SDから選ぶ…",u8"Von SD wählen...",u8"Choisir sur SD...",u8"Elegir de SD...",u8"Scegli da SD...",u8"Kiezen van SD...",u8"从 SD 卡选择…",u8"從 SD 卡選擇…",u8"SD에서 선택..."),
		T10("Original",u8"オリジナル",u8"Original",u8"Original",u8"Original",u8"Originale",u8"Origineel",u8"原版",u8"原版",u8"원본"),
		T10("Up Dog preset",u8"Up Dogプリセット",u8"Up-Dog-Vorgabe",u8"Préréglage Up Dog",u8"Preajuste Up Dog",u8"Predefinito Up Dog",u8"Up Dog-voorinstelling",u8"Up Dog 预设",u8"Up Dog 預設",u8"Up Dog 프리셋"),
		T10("OG colours (custom)",u8"元の色（カスタム）",u8"Originalfarben (eigen)",u8"Couleurs d’origine (perso)",u8"Colores originales (personal.)",u8"Colori originali (pers.)",u8"Originele kleuren (eigen)",u8"原始颜色（自定义）",u8"原始顏色（自訂）",u8"원래 색상 (사용자)"),
		T10("Custom blue/green/blue",u8"カスタム 青/緑/青",u8"Eigen Blau/Grün/Blau",u8"Perso bleu/vert/bleu",u8"Personal. azul/verde/azul",u8"Pers. blu/verde/blu",u8"Eigen blauw/groen/blauw",u8"自定义 蓝/绿/蓝",u8"自訂 藍/綠/藍",u8"사용자 파랑/초록/파랑"),
		T10("Second purple",u8"2番目の紫",u8"Zweite violette Blase",u8"Deuxième bulle violette",u8"Segunda morada",u8"Seconda viola",u8"Tweede paarse",u8"第二个紫色气泡",u8"第二個紫色泡泡",u8"두 번째 보라 말풍선"),
		T10("Third green",u8"3番目の緑",u8"Dritte grüne Blase",u8"Troisième bulle verte",u8"Tercera verde",u8"Terza verde",u8"Derde groene",u8"第三个绿色气泡",u8"第三個綠色泡泡",u8"세 번째 초록 말풍선"),
		T10("Restore Up Dog preset",u8"Up Dogを復元",u8"Up-Dog-Vorgabe wiederherstellen",u8"Restaurer Up Dog",u8"Restaurar Up Dog",u8"Ripristina Up Dog",u8"Up Dog herstellen",u8"恢复 Up Dog 预设",u8"還原 Up Dog 預設",u8"Up Dog 프리셋 복원")
		,T10("Cancel",u8"キャンセル",u8"Abbrechen",u8"Annuler",u8"Cancelar",u8"Annulla",u8"Annuleren",u8"取消",u8"取消",u8"취소")
		,T10("Done",u8"決定",u8"Fertig",u8"Terminé",u8"Hecho",u8"Fine",u8"Klaar",u8"完成",u8"完成",u8"완료")
		,T10("Edit",u8"編集",u8"Bearbeiten",u8"Modifier",u8"Editar",u8"Modifica",u8"Bewerken",u8"编辑",u8"編輯",u8"편집")
		,T10("Text",u8"テキスト",u8"Text",u8"Texte",u8"Texto",u8"Testo",u8"Tekst",u8"文本",u8"文字",u8"텍스트")
		,T10("News article",u8"ニュース記事",u8"Nachrichtenartikel",u8"Article d’actualité",u8"Artículo de noticias",u8"Articolo di notizie",u8"Nieuwsartikel",u8"新闻文章",u8"新聞文章",u8"뉴스 기사")
		,T10("Forecast city",u8"予報都市",u8"Vorhersageort",u8"Ville météo",u8"Ciudad del pronóstico",u8"Città meteo",u8"Weerplaats",u8"预报城市",u8"預報城市",u8"예보 도시")
		,T10("Nintendo icon message",u8"Nintendoアイコンメッセージ",u8"Nintendo-Icon-Nachricht",u8"Message icône Nintendo",u8"Mensaje icono Nintendo",u8"Messaggio icona Nintendo",u8"Nintendo-pictogramtekst",u8"任天堂图标消息",u8"任天堂圖示訊息",u8"닌텐도 아이콘 메시지")
		,T10("First blue bubble",u8"最初の青い吹き出し",u8"Erste blaue Blase",u8"Première bulle bleue",u8"Primera burbuja azul",u8"Prima bolla blu",u8"Eerste blauwe ballon",u8"第一个蓝色气泡",u8"第一個藍色泡泡",u8"첫 파란 말풍선")
		,T10("Second purple bubble",u8"2番目の紫の吹き出し",u8"Zweite violette Blase",u8"Deuxième bulle violette",u8"Segunda burbuja morada",u8"Seconda bolla viola",u8"Tweede paarse ballon",u8"第二个紫色气泡",u8"第二個紫色泡泡",u8"두 번째 보라 말풍선")
		,T10("Green question bubble",u8"緑の質問吹き出し",u8"Grüne Frageblase",u8"Bulle de question verte",u8"Burbuja de pregunta verde",u8"Bolla domanda verde",u8"Groene vraagballon",u8"绿色问题气泡",u8"綠色問題泡泡",u8"초록 질문 말풍선")
		,T10("Third green bubble",u8"3番目の緑の吹き出し",u8"Dritte grüne Blase",u8"Troisième bulle verte",u8"Tercera burbuja verde",u8"Terza bolla verde",u8"Derde groene ballon",u8"第三个绿色气泡",u8"第三個綠色泡泡",u8"세 번째 초록 말풍선")
		,T10("Final blue bubble",u8"最後の青い吹き出し",u8"Letzte blaue Blase",u8"Dernière bulle bleue",u8"Burbuja azul final",u8"Bolla blu finale",u8"Laatste blauwe ballon",u8"最后蓝色气泡",u8"最後藍色泡泡",u8"마지막 파란 말풍선")
		,T10("Choose Nintendo icon image",u8"Nintendoアイコン画像を選択",u8"Nintendo-Icon-Bild wählen",u8"Choisir l’image Nintendo",u8"Elegir imagen Nintendo",u8"Scegli immagine Nintendo",u8"Nintendo-afbeelding kiezen",u8"选择任天堂图标图片",u8"選擇任天堂圖示圖片",u8"닌텐도 아이콘 이미지 선택")
		,T10("Choose Photo Channel image",u8"写真チャンネル画像を選択",u8"Fotokanal-Bild wählen",u8"Choisir l’image Photo",u8"Elegir imagen del Canal Fotos",u8"Scegli immagine Canale Foto",u8"Fotokanaal-afbeelding kiezen",u8"选择照片频道图片",u8"選擇相片頻道圖片",u8"사진 채널 이미지 선택")
		,T10("<No image / use original>",u8"＜画像なし / オリジナル＞",u8"<Kein Bild / Original>",u8"<Aucune image / original>",u8"<Sin imagen / original>",u8"<Nessuna immagine / originale>",u8"<Geen afbeelding / origineel>",u8"<无图片 / 使用原版>",u8"<無圖片 / 使用原版>",u8"<이미지 없음 / 원본>")
		,T10("A: Select     B: Cancel",u8"A: 選択     B: キャンセル",u8"A: Wählen     B: Abbrechen",u8"A : Choisir     B : Annuler",u8"A: Elegir     B: Cancelar",u8"A: Scegli     B: Annulla",u8"A: Kiezen     B: Annuleren",u8"A：选择     B：取消",u8"A：選擇     B：取消",u8"A: 선택     B: 취소")
		,T10("B: Cancel     +: Done",u8"B: キャンセル     +: 決定",u8"B: Abbrechen     +: Fertig",u8"B : Annuler     + : Terminé",u8"B: Cancelar     +: Hecho",u8"B: Annulla     +: Fine",u8"B: Annuleren     +: Klaar",u8"B：取消     +：完成",u8"B：取消     +：完成",u8"B: 취소     +: 완료")
		,T10("Back",u8"戻る",u8"Zurück",u8"Retour",u8"Volver",u8"Indietro",u8"Terug",u8"返回",u8"返回",u8"뒤로")
		,T10("Shift",u8"シフト",u8"Umschalt",u8"Majuscule",u8"Mayús",u8"Maiusc",u8"Shift",u8"换档",u8"Shift",u8"Shift")
		,T10("Space",u8"スペース",u8"Leerzeichen",u8"Espace",u8"Espacio",u8"Spazio",u8"Spatie",u8"空格",u8"空格",u8"공백")
		,T10("Sunny",u8"晴れ",u8"Sonnig",u8"Ensoleillé",u8"Soleado",u8"Soleggiato",u8"Zonnig",u8"晴",u8"晴",u8"맑음")
		,T10("Cloudy",u8"くもり",u8"Bewölkt",u8"Nuageux",u8"Nublado",u8"Nuvoloso",u8"Bewolkt",u8"多云",u8"多雲",u8"흐림")
		,T10("Mostly cloudy",u8"ほぼくもり",u8"Meist bewölkt",u8"Très nuageux",u8"Mayormente nublado",u8"Molto nuvoloso",u8"Overwegend bewolkt",u8"大部多云",u8"大致多雲",u8"대체로 흐림")
		,T10("Fog",u8"霧",u8"Nebel",u8"Brouillard",u8"Niebla",u8"Nebbia",u8"Mist",u8"雾",u8"霧",u8"안개")
		,T10("Showers",u8"にわか雨",u8"Schauer",u8"Averses",u8"Chubascos",u8"Rovesci",u8"Buien",u8"阵雨",u8"陣雨",u8"소나기")
		,T10("Rain",u8"雨",u8"Regen",u8"Pluie",u8"Lluvia",u8"Pioggia",u8"Regen",u8"雨",u8"雨",u8"비")
		,T10("Mostly cloudy with showers",u8"くもり時々にわか雨",u8"Meist bewölkt mit Schauern",u8"Très nuageux avec averses",u8"Mayormente nublado con chubascos",u8"Molto nuvoloso con rovesci",u8"Overwegend bewolkt met buien",u8"大部多云有阵雨",u8"大致多雲有陣雨",u8"대체로 흐리고 소나기")
		,T10("Thunder",u8"雷雨",u8"Gewitter",u8"Orage",u8"Tormenta",u8"Temporale",u8"Onweer",u8"雷雨",u8"雷雨",u8"뇌우")
		,T10("Partly sunny with thunderstorms",u8"晴れ時々雷雨",u8"Teilweise sonnig mit Gewittern",u8"Éclaircies avec orages",u8"Parcialmente soleado con tormentas",u8"Parzialmente soleggiato con temporali",u8"Gedeeltelijk zonnig met onweer",u8"局部晴有雷暴",u8"局部晴有雷暴",u8"부분적으로 맑고 뇌우")
		,T10("Mostly cloudy with thunderstorms",u8"くもり時々雷雨",u8"Meist bewölkt mit Gewittern",u8"Très nuageux avec orages",u8"Mayormente nublado con tormentas",u8"Molto nuvoloso con temporali",u8"Overwegend bewolkt met onweer",u8"大部多云有雷暴",u8"大致多雲有雷暴",u8"대체로 흐리고 뇌우")
		,T10("Snow",u8"雪",u8"Schnee",u8"Neige",u8"Nieve",u8"Neve",u8"Sneeuw",u8"雪",u8"雪",u8"눈")
		,T10("Partly sunny with flurries",u8"晴れ時々雪",u8"Teilweise sonnig mit Schneeschauern",u8"Éclaircies avec averses de neige",u8"Parcialmente soleado con nieve",u8"Parzialmente soleggiato con neve",u8"Gedeeltelijk zonnig met sneeuwbuien",u8"局部晴有阵雪",u8"局部晴有陣雪",u8"부분적으로 맑고 눈발")
		,T10("Rain and snow",u8"みぞれ",u8"Regen und Schnee",u8"Pluie et neige",u8"Lluvia y nieve",u8"Pioggia e neve",u8"Regen en sneeuw",u8"雨夹雪",u8"雨夾雪",u8"비와 눈")
		,T10("Sleet",u8"凍雨",u8"Schneeregen",u8"Grésil",u8"Aguanieve",u8"Nevischio",u8"Natte sneeuw",u8"雨雪",u8"雨雪",u8"진눈깨비")
		,T10("Partly cloudy",u8"晴れ時々くもり",u8"Teilweise bewölkt",u8"Partiellement nuageux",u8"Parcialmente nublado",u8"Parzialmente nuvoloso",u8"Gedeeltelijk bewolkt",u8"局部多云",u8"局部多雲",u8"구름 조금")
		,T10("Partly sunny",u8"晴れ間あり",u8"Teilweise sonnig",u8"Éclaircies",u8"Parcialmente soleado",u8"Parzialmente soleggiato",u8"Gedeeltelijk zonnig",u8"局部晴",u8"局部晴",u8"부분적으로 맑음")
		,T10("Partly sunny with rain",u8"晴れ時々雨",u8"Teilweise sonnig mit Regen",u8"Éclaircies avec pluie",u8"Parcialmente soleado con lluvia",u8"Parzialmente soleggiato con pioggia",u8"Gedeeltelijk zonnig met regen",u8"局部晴有雨",u8"局部晴有雨",u8"부분적으로 맑고 비")
		,T10("No PNG/JPEG images found     B: Cancel",u8"PNG/JPEG画像がありません     B: キャンセル",u8"Keine PNG/JPEG-Bilder     B: Abbrechen",u8"Aucune image PNG/JPEG     B : Annuler",u8"No hay imágenes PNG/JPEG     B: Cancelar",u8"Nessuna immagine PNG/JPEG     B: Annulla",u8"Geen PNG/JPEG-afbeeldingen     B: Annuleren",u8"未找到 PNG/JPEG 图片     B：取消",u8"找不到 PNG/JPEG 圖片     B：取消",u8"PNG/JPEG 이미지 없음     B: 취소")
		,T10("SD card unavailable     B: Cancel",u8"SDカードを使用できません     B: キャンセル",u8"SD-Karte nicht verfügbar     B: Abbrechen",u8"Carte SD indisponible     B : Annuler",u8"Tarjeta SD no disponible     B: Cancelar",u8"Scheda SD non disponibile     B: Annulla",u8"SD-kaart niet beschikbaar     B: Annuleren",u8"SD 卡不可用     B：取消",u8"SD 卡無法使用     B：取消",u8"SD 카드 사용 불가     B: 취소")
		,T10("Time of day",u8"時間帯",u8"Tageszeit",u8"Moment de la journée",u8"Hora del día",u8"Ora del giorno",u8"Tijdstip",u8"时段",u8"時段",u8"시간대")
		,T10("Day",u8"昼",u8"Tag",u8"Jour",u8"Día",u8"Giorno",u8"Dag",u8"白天",u8"白天",u8"낮")
		,T10("Night",u8"夜",u8"Nacht",u8"Nuit",u8"Noche",u8"Notte",u8"Nacht",u8"夜间",u8"夜間",u8"밤")
		,T10("Automatic",u8"自動",u8"Automatisch",u8"Automatique",u8"Automático",u8"Automatico",u8"Automatisch",u8"自动",u8"自動",u8"자동")
	};
#undef T10

	struct ExtraTranslation { const char *english; const char *portuguese; const char *swedish; };
	const ExtraTranslation ExtraText[] = {
#include "localization_extra.inc"
	};
	const Utf8Translation CompleteText[] = {
#include "localization_catalogue.inc"
	};

	char16 Decoded[ Localization::KeyCount ][ LanguageCount ][ MaxLocalizedCharacters ];
	bool DecodedReady[ Localization::KeyCount ][ LanguageCount ] = {};

	int LanguageIndex( int language )
	{
		return language >= 0 && language < LanguageCount
			? language : CONF_LANG_ENGLISH;
	}

	void DecodeUtf8( const char *source, char16 *destination, size_t capacity )
	{
		size_t written = 0;
		const unsigned char *input = (const unsigned char *)source;
		while( *input && written + 1 < capacity )
		{
			u32 codepoint;
			if( input[ 0 ] < 0x80 )
			{
				codepoint = *input++;
			}
			else if( ( input[ 0 ] & 0xe0 ) == 0xc0 && input[ 1 ] )
			{
				codepoint = ( ( input[ 0 ] & 0x1f ) << 6 ) | ( input[ 1 ] & 0x3f );
				input += 2;
			}
			else if( ( input[ 0 ] & 0xf0 ) == 0xe0 && input[ 1 ] && input[ 2 ] )
			{
				codepoint = ( ( input[ 0 ] & 0x0f ) << 12 )
					| ( ( input[ 1 ] & 0x3f ) << 6 ) | ( input[ 2 ] & 0x3f );
				input += 3;
			}
			else
			{
				// None of these UI strings needs supplementary-plane glyphs.
				codepoint = '?';
				++input;
			}
			destination[ written++ ] = (char16)codepoint;
		}
		destination[ written ] = 0;
	}
}

#include <map>
#include <string>
#include <vector>

const char16 *Localization::GetText( const char *english )
{
	// Layouts retain pointers. Keep each language's decoded text alive across
	// subsequent lookups and resource reloads; do not reuse a scratch buffer.
	typedef std::pair<int, std::string> CacheKey;
	static std::map<CacheKey, std::vector<char16> > cache;
	const CacheKey key( CurrentLanguage(), english ? english : "" );
	std::vector<char16> &decoded = cache[key];
	if( decoded.empty() )
	{
		const char *text = GetUtf8( english );
		decoded.resize( strlen(text) + 1 );
		DecodeUtf8( text, &decoded[0], decoded.size() );
	}
	return &decoded[0];
}

const char16 *Localization::GetMenuMessage( unsigned int index )
{
	// Public v514 message IDs, before BMG's old-version index resolver.
	// Other messages continue using Nintendo's selected-language BMG.
	const char *key = NULL;
	switch( index )
	{
	case 0: key = "Disc Channel"; break;
	case 1: case 16: case 168: key = "Wii Menu"; break;
	case 2: key = "Start"; break;
	case 17: key = "Calendar"; break;
	case 35: case 79: case 165: case 252: key = "Back"; break;
	case 47: case 189: case 260: key = "Erase"; break;
	case 96: case 105: key = "Cancel"; break;
	case 110: key = "Mon"; break;
	case 111: key = "Tue"; break;
	case 112: key = "Wed"; break;
	case 113: key = "Thu"; break;
	case 114: key = "Fri"; break;
	case 115: key = "Sat"; break;
	case 116: key = "Sun"; break;
	case 156: key = "Blocks Open: "; break;
	case 164: key = "Close"; break;
	case 167: case 259: key = "Move"; break;
	case 178: case 258: key = "Copy"; break;
	case 253: key = "Data Management"; break;
	case 254: key = "Save Data"; break;
	case 255: key = "Channels"; break;
	case 316: key = "Wii Settings"; break;
	case 318: key = "SD Card"; break;
	case 321: key = "Yes"; break;
	case 322: key = "No"; break;
	default: return NULL;
	}
	return GetText( key );
}

namespace {
struct ChannelTextIndexEntry { u32 hash; unsigned short row; unsigned short language; };
const ChannelTextIndexEntry ChannelTextIndex[] = {
#include "localization_channel_index.inc"
};
const char *const ChannelTextAliases[] = {
#include "localization_channel_aliases.inc"
};
std::basic_string<char16> NormalizeChannelText(const char16 *text)
{
	std::basic_string<char16> out;
	bool space = false;
	for(; text && *text; ++text) {
		if(*text == ' ' || *text == '\n' || *text == '\r' || *text == '\t') {
			space = !out.empty(); continue;
		}
		if(space) out += ' ';
		space = false; out += *text;
	}
	return out;
}
}
const char16 *Localization::TranslateChannelText( const char16 *authored )
{
	if( !authored || !*authored ) return NULL;
	// Only called while loading authored channel layouts, before custom data
	// is injected. Never translate user news, cities, weather names or messages.
	const std::basic_string<char16> normalized = NormalizeChannelText(authored);
	u32 hash = 2166136261u;
	for(size_t i=0;i<normalized.size();++i) hash=(hash ^ (u32)normalized[i])*16777619u;
	const size_t count=sizeof(ChannelTextIndex)/sizeof(ChannelTextIndex[0]);
	size_t low=0,high=count;
	while(low<high) {
		const size_t mid=low+(high-low)/2;
		if(ChannelTextIndex[mid].hash<hash) low=mid+1; else high=mid;
	}
	char16 candidate[1024];
	for(size_t index=low;index<count && ChannelTextIndex[index].hash==hash;++index)
	{
		const u32 row=ChannelTextIndex[index].row;
		const int language=ChannelTextIndex[index].language;
		{
			const char *source = language < LanguageCount ? CompleteText[row].text[language]
				: ChannelTextAliases[language-LanguageCount];
			if( !source || strlen(source) >= sizeof(candidate) / sizeof(candidate[0]) ) continue;
			DecodeUtf8( source, candidate, sizeof(candidate) / sizeof(candidate[0]) );
			if( NormalizeChannelText(candidate) != normalized ) continue;
			const char16 *translated = GetText(CompleteText[row].english);
			// Preserve native layout/text metrics when the desired language is
			// already present. Padding and authored line breaks are not content.
			if( NormalizeChannelText(translated) == normalized ) return authored;
			typedef std::pair<int,std::basic_string<char16> > Key;
			static std::map<Key,std::basic_string<char16> > wrapped;
			const Key key(CurrentLanguage(),std::basic_string<char16>(authored));
			std::basic_string<char16> &result = wrapped[key];
			if(result.empty()) {
				result = translated;
				// Match the original paragraph's line budget instead of shrinking
				// a translated help paragraph into one tiny horizontal line.
				size_t longest=0, line=0, lines=1;
				for(const char16 *p=authored;*p;++p) {
					if(*p=='\n') { longest=std::max(longest,line);line=0;++lines; }
					else ++line;
				}
				longest=std::max(longest,line);
				if(lines>1 && longest>0) {
					size_t start=0,lastSpace=std::basic_string<char16>::npos;
					for(size_t i=0;i<result.size();++i) {
						if(result[i]==' ') lastSpace=i;
						if(i-start>=longest && lastSpace!=std::basic_string<char16>::npos && lastSpace>start) {
							result[lastSpace]='\n'; start=lastSpace+1; lastSpace=std::basic_string<char16>::npos;
						}
					}
				}
			}
			return result.c_str();
		}
	}
	return NULL;
}

const char16 *Localization::Get( Key key )
{
	return Get( key, CurrentLanguage() );
}

const char16 *Localization::Get( Key key, int language )
{
	if( key < 0 || key >= KeyCount )
		key = ReturnToLoader;
	const int languageIndex = LanguageIndex( language );
	if( !DecodedReady[ key ][ languageIndex ] )
	{
		const char *text = languageIndex < 10 ? Text[ key ][ languageIndex ]
			: GetUtf8( Text[ key ][ CONF_LANG_ENGLISH ], languageIndex );
		DecodeUtf8( text, Decoded[ key ][ languageIndex ],
			MaxLocalizedCharacters );
		DecodedReady[ key ][ languageIndex ] = true;
	}
	return Decoded[ key ][ languageIndex ];
}

const char *Localization::GetUtf8( const char *english )
{
	return GetUtf8( english, CurrentLanguage() );
}

int Localization::CurrentLanguage()
{
	const int language = Settings::uiLanguage == -1 ? CONF_GetLanguage() : Settings::uiLanguage;
	return language >= 0 && language <= 6 ? language : CONF_LANG_ENGLISH;
}

int Localization::NativeLanguage()
{
	const int language = CurrentLanguage();
	// Nintendo archives/IMET have ten slots, not the extended UI catalogue.
	return language < 10 ? language : CONF_LANG_ENGLISH;
}

const char *Localization::LanguageCode()
{
	static const char *const codes[LanguageCount] = {
		"ja","en","de","fr","es","it","nl","zh-CN","zh-TW","ko",
		"pt","sv","da","no","fi","is","pl","cs","sk","hu","ro","el",
		"ru","uk","bg","hr","sl","sq","et","lv","lt","ga","cy","mt",
		"lb","ca","eu","gl","be","tr","hy","ka","az","gd","fy","br"
	};
	return codes[CurrentLanguage()];
}

const char *Localization::LanguageName( int language )
{
	static const char *const names[] = {
#include "localization_languages.inc"
	};
	return language < 0 ? GetUtf8( "Use Wii language" ) : names[ LanguageIndex(language) ];
}

const char *Localization::GetUtf8( const char *english, int selectedLanguage )
{
	if( !english ) return "";
	const int language = LanguageIndex( selectedLanguage );
	for( u32 i = 0; i < sizeof( CompleteText ) / sizeof( CompleteText[ 0 ] ); ++i )
		if( !strcmp( english, CompleteText[ i ].english ) )
			return CompleteText[ i ].text[ language ];
	return english;
}
