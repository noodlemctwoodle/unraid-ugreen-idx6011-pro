<?php
/*
 * plugin/src/webgui/idxcp-inc.php
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Shared helpers for the UGREEN iDX6011 Pro settings tabs (Screen + Lighting).
 * Required by each tab .page so the colour/select markup and cfg access stay in
 * one place. Controls are NATIVE Unraid form elements (no custom styling); the
 * only asset is idxcp.js (loaded per tab as an external <script src>).
 */

/* parsed settings.cfg (empty array if absent/invalid) */
function idx_cfg() {
    $f = '/boot/config/plugins/ugreen-idx6011-pro/panel/settings.cfg';
    return is_file($f) ? (@parse_ini_file($f) ?: []) : [];
}
function idx_val($cfg, $k, $def = '') { return isset($cfg[$k]) ? $cfg[$k] : $def; }

/* saved theme files (panel/themes/*.cfg) -> [{name, keys:{K:V,...}}, ...] for the
 * Load-theme dropdown. A theme is just the Theme-tab keys; share one by copying its
 * .cfg (the themes folder is on the flash `config` share).
 *
 * Hardened: a theme file can arrive by any means (SMB-copied in), so every value is
 * validated here at the read boundary — colours must be 6-hex, numerics must be in
 * range, FONT must be a known name, and the file name itself must be tame. Anything
 * else is dropped, so a rogue .cfg can only ever surface safe values (the panel also
 * clamps everything again on load). */
function idx_themes() {
    $dir   = '/boot/config/plugins/ugreen-idx6011-pro/panel/themes';
    $cols  = ['COL_ACCENT','COL_GRAD_A','COL_GRAD_B','COL_BG','COL_CARD','COL_TEXT',
              'COL_TITLE','COL_DIM','COL_OK','COL_WARN','COL_BAD'];
    $nums  = ['HEAD_SCALE'=>[70,200],'TEXT_SCALE'=>[70,200],'CARD_OPACITY'=>[10,100],'BG_DIM'=>[0,80]];
    $fonts = ['RobotoCondensed','Inter','JetBrainsMono','BarlowSemiCond','Montserrat'];
    $out = [];
    foreach (glob("$dir/*.cfg") ?: [] as $f) {
        $name = basename($f, '.cfg');
        if (!preg_match('/^[A-Za-z0-9 _-]{1,40}$/', $name)) continue;   /* skip odd file names */
        $ini = @parse_ini_file($f) ?: [];
        $t = [];
        foreach ($cols as $k)
            if (isset($ini[$k]) && preg_match('/^#?[0-9a-fA-F]{6}$/', (string)$ini[$k]))
                $t[$k] = strtolower(ltrim((string)$ini[$k], '#'));
        foreach ($nums as $k => $r)
            if (isset($ini[$k]) && is_numeric($ini[$k]) && (int)$ini[$k] >= $r[0] && (int)$ini[$k] <= $r[1])
                $t[$k] = (string)(int)$ini[$k];
        if (isset($ini['FONT']) && in_array($ini['FONT'], $fonts, true)) $t['FONT'] = $ini['FONT'];
        if ($t) $out[] = ['name' => $name, 'keys' => $t];
    }
    return $out;
}

/* cache-busted webGUI asset URL */
function idx_asset($f) {
    return "/plugins/ugreen-idx6011-pro/$f?v=" .
           (@filemtime("/usr/local/emhttp/plugins/ugreen-idx6011-pro/$f") ?: '1');
}

/* real network interfaces for the "primary interface" picker */
function idx_ifaces() {
    $r = [];
    foreach (glob('/sys/class/net/*') as $p) {
        $n = basename($p);
        if ($n === 'lo' || $n === 'bonding_masters') continue;
        if (preg_match('/^(veth|docker|tunl|sit|vnet|br-|dummy|wg)/', $n)) continue;
        $r[] = $n;
    }
    sort($r);
    return $r;
}

function idx_opts($items, $cur) {
    foreach ($items as $v => $lbl)
        echo '<option value="' . htmlspecialchars($v) . '"' .
             ((string)$cur === (string)$v ? ' selected' : '') . '>' .
             htmlspecialchars($lbl) . '</option>';
}

/* native colour control: <input type=color> swatch + text hex field, synced by
 * idxcp.js. The text field is the submitted value (BARE 6-hex); $cls tags the
 * section (idx-theme / idx-led) for the per-tab Restore-defaults button. */
function idx_colorField($cfg, $name, $default, $cls) {
    $def = strtolower(ltrim($default, '#'));
    $v   = isset($cfg[$name]) ? strtolower(ltrim(trim($cfg[$name]), '#')) : '';
    if (!preg_match('/^[0-9a-f]{6}$/', $v)) $v = $def;
    $n = htmlspecialchars($name);
    echo '<span class="idxcp">'
       . '<input type="color" class="idxcp-chip" value="#'.$v.'" aria-label="'.$n.' '._('colour').'" title="'._('Pick colour').'"> '
       . '<input type="text" class="idxcp-hex '.$cls.'" name="'.$n.'" value="'.$v.'" data-default="'.$def.'"'
       .        ' size="8" maxlength="7" spellcheck="false" autocomplete="off" aria-label="'.$n.' hex">'
       . '</span>';
}
