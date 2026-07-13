<?php
/*
 * plugin/src/webgui/theme.php
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Validated theme-file writer for the Display > Theme tab. POST (urlencoded, with
 * Unraid's csrf_token) a `name` plus the theme keys; this writes ONLY validated
 * values to panel/themes/<name>.cfg and returns JSON {ok,...}. Belt-and-braces to
 * idx_themes()'s read-side validation: a theme can never be written with a rogue
 * name, a non-hex colour, an out-of-range size/opacity, or an unknown font. (CSRF +
 * login are enforced by Unraid's auto-prepend, same as update.php.)
 */
header('Content-Type: application/json');

$name = isset($_POST['name']) ? trim((string)$_POST['name']) : '';
if (!preg_match('/^[A-Za-z0-9 _-]{1,40}$/', $name)) { echo json_encode(['ok'=>false,'err'=>'bad name']); exit; }
$name = preg_replace('/\s+/', '-', $name);

$cols  = ['COL_ACCENT','COL_GRAD_A','COL_GRAD_B','COL_BG','COL_CARD','COL_TEXT',
          'COL_TITLE','COL_DIM','COL_OK','COL_WARN','COL_BAD'];
$nums  = ['HEAD_SCALE'=>[70,200],'TEXT_SCALE'=>[70,200],'CARD_OPACITY'=>[10,100],'BG_DIM'=>[0,80]];
$fonts = ['RobotoCondensed','Inter','JetBrainsMono','BarlowSemiCond','Montserrat'];

$out = [];
foreach ($cols as $k)
    if (isset($_POST[$k]) && preg_match('/^#?[0-9a-fA-F]{6}$/', (string)$_POST[$k]))
        $out[$k] = strtolower(ltrim((string)$_POST[$k], '#'));
foreach ($nums as $k => $r)
    if (isset($_POST[$k]) && is_numeric($_POST[$k]) && (int)$_POST[$k] >= $r[0] && (int)$_POST[$k] <= $r[1])
        $out[$k] = (int)$_POST[$k];
if (isset($_POST['FONT']) && in_array($_POST['FONT'], $fonts, true)) $out['FONT'] = $_POST['FONT'];

if (!$out) { echo json_encode(['ok'=>false,'err'=>'no valid theme values']); exit; }

$dir = '/boot/config/plugins/ugreen-idx6011-pro/panel/themes';
@mkdir($dir, 0755, true);
$text = '';
foreach ($out as $k => $v) $text .= $k . '="' . $v . "\"\n";
if (@file_put_contents("$dir/$name.cfg", $text) === false) { echo json_encode(['ok'=>false,'err'=>'write failed']); exit; }
echo json_encode(['ok'=>true, 'name'=>$name, 'keys'=>count($out)]);
