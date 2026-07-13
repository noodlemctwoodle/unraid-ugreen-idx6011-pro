/*
 * plugin/src/webgui/idxcp.js
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Minimal glue for the settings page's colour controls. Uses NATIVE Unraid form
 * controls (a plain <input type=color> + a text field) — no custom styling, so the
 * page inherits the active Unraid theme. This script only:
 *   - keeps the swatch and the hex field in sync,
 *   - enables Apply on change (Unraid's own listener ignores <input type=color>),
 *   - restores per-section defaults for idxReset() buttons.
 * Loaded as an external <script src> (an inline <script> after a markdown="1" form
 * is mangled by Unraid's MarkdownExtra pass). Vanilla JS, no jQuery dependency.
 */
(function(){
  "use strict";
  function norm(raw){ var s=String(raw==null?'':raw).trim().replace(/^#+/,'').toLowerCase();
    if(/^[0-9a-f]{3}$/.test(s)) s=s.charAt(0)+s.charAt(0)+s.charAt(1)+s.charAt(1)+s.charAt(2)+s.charAt(2);
    return /^[0-9a-f]{6}$/.test(s)?s:null; }
  function markChanged(el){ var f=el.closest('form'); if(f){ var a=f.querySelectorAll('input[value="Apply"]');
    for(var i=0;i<a.length;i++) a[i].disabled=false; } try{ formHasUnsavedChanges=true; }catch(e){} }
  function setup(cp){
    var chip=cp.querySelector('.idxcp-chip'), text=cp.querySelector('.idxcp-hex');
    if(!chip||!text) return;
    var syncing=false;
    chip.addEventListener('input', function(){                 // picked via the swatch
      var n=norm(chip.value); if(!n) return;
      syncing=true; text.value=n;
      text.dispatchEvent(new Event('change',{bubbles:true})); syncing=false; markChanged(text);
    });
    text.addEventListener('input', function(){                 // typed a hex
      if(syncing) return; var n=norm(text.value); if(n){ chip.value='#'+n; markChanged(text); }
    });
    text.addEventListener('change', function(){                // settle -> normalise
      if(syncing) return; var n=norm(text.value); if(n){ text.value=n; chip.value='#'+n; }
    });
    var init=norm(text.value); if(init){ text.value=init; chip.value='#'+init; }
  }
  window.idxReset=function(cls){
    var hx=document.querySelectorAll('.idxcp-hex.'+cls);
    for(var i=0;i<hx.length;i++){
      var cp=hx[i].closest('.idxcp'), chip=cp&&cp.querySelector('.idxcp-chip'),
          d=(hx[i].getAttribute('data-default')||'').toLowerCase();
      hx[i].value=d; if(chip) chip.value='#'+d;
      hx[i].dispatchEvent(new Event('change',{bubbles:true})); markChanged(hx[i]);
    }
    var sel=document.querySelectorAll('select.'+cls+'[data-default]');
    for(var j=0;j<sel.length;j++){ sel[j].value=sel[j].getAttribute('data-default');
      sel[j].dispatchEvent(new Event('change',{bubbles:true})); markChanged(sel[j]); }
  };
  /* wallpaper / header-logo upload controls (.idxup) -> upload.php, then the panel
   * restarts to apply. Separate from the form Apply (a file, not a cfg key). */
  function setupUploads(){
    var ups=document.querySelectorAll('.idxup');
    for(var i=0;i<ups.length;i++){ (function(up){
      var kind=up.getAttribute('data-kind'), file=up.querySelector('input[type=file]'),
          note=up.querySelector('.idxup-note'), btns=up.querySelectorAll('input[type=button]'),
          base='/plugins/ugreen-idx6011-pro/upload.php?kind='+kind;
      if(btns[0]) btns[0].addEventListener('click',function(){
        if(!file.files||!file.files[0]){ note.textContent='choose a file first'; return; }
        var fd=new FormData(); fd.append('file', file.files[0]); note.textContent='uploading…';
        fetch(base,{method:'POST',body:fd}).then(function(r){return r.json();})
          .then(function(j){ note.textContent=j.ok?'uploaded — panel updating…':('failed: '+(j.err||'')); if(j.ok) file.value=''; })
          .catch(function(){ note.textContent='upload failed'; });
      });
      if(btns[1]) btns[1].addEventListener('click',function(){
        note.textContent='clearing…';
        fetch(base+'&clear=1',{method:'POST'}).then(function(r){return r.json();})
          .then(function(j){ note.textContent=j.ok?'cleared — panel updating…':'failed'; })
          .catch(function(){ note.textContent='failed'; });
      });
    })(ups[i]); }
  }
  /* shared control CSS so the Lighting tab and the Display > Theme tab render with
   * the identical label|control grid design (idxlayout.js reuses this class too). */
  function injectCss(){
    if(document.getElementById('idxcp-css')) return;
    var s=document.createElement('style'); s.id='idxcp-css';
    s.textContent =
      /* label | control grid; the label column auto-sizes to the widest label so
       * rows line up. Cells stretch to fill their track + row height, and each
       * carries a bottom border, so the two cells form ONE full-width row line. */
      '.idxth-grid{display:grid;grid-template-columns:auto 1fr;column-gap:16px;row-gap:0;width:100%;max-width:680px}'+
      '.idxth-grid > label,.idxth-grid > select,.idxth-grid > input,.idxth-grid > .idxcp{border-bottom:1px solid rgba(128,128,128,.22);padding:9px 0;box-sizing:border-box;justify-self:stretch}'+
      /* drop the native control box (Unraid draws a darker border) so only the one
       * faint grey row line shows; keep the bottom border set above */
      '.idxth-grid > select,.idxth-grid > input{border-top:0;border-left:0;border-right:0;box-shadow:none;background:transparent}'+
      '.idxth-grid > label,.idxth-grid > .idxcp{display:flex;align-items:center}'+
      '.idxth-note{grid-column:1/-1;margin:1px 0 7px;padding:0;opacity:.7;font-size:.85em;line-height:1.4}'+
      '.idxth-grid label{opacity:.9;font-weight:600}'+
      /* swatch + hex on ONE line, left-aligned within the (full-width) cell */
      '.idxcp{gap:9px}'+
      '.idxcp-chip{flex:0 0 auto;width:48px;height:26px;padding:0;border:1px solid rgba(128,128,128,.5);border-radius:13px;background:none;cursor:pointer;overflow:hidden}'+
      /* zero the native swatch inset so the colour fills the whole pill */
      '.idxcp-chip::-webkit-color-swatch-wrapper{padding:0}'+
      '.idxcp-chip::-webkit-color-swatch{border:none;border-radius:12px}'+
      '.idxcp-chip::-moz-color-swatch{border:none;border-radius:12px}'+
      '.idxcp-hex{flex:0 0 auto;width:90px;border:0;box-shadow:none;background:transparent}'+
      '.idxth-sec{grid-column:1/-1;border:none;margin:13px 0 3px;padding:0;font-weight:600;opacity:.8;letter-spacing:.4px;text-transform:uppercase;font-size:.85em}';
    document.head.appendChild(s);
  }
  function initAll(){ injectCss(); var cps=document.querySelectorAll('.idxcp'); for(var i=0;i<cps.length;i++) setup(cps[i]); setupUploads(); }
  if(document.readyState==='loading') document.addEventListener('DOMContentLoaded',initAll); else initAll();
})();
