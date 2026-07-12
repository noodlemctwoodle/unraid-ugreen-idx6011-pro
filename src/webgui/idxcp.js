/*
 * plugin/src/webgui/idxcp.js
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Colour-picker control for the UGREEN iDX6011 Pro settings page.
 * Loaded as an external script (an inline <script> after a markdown="1" form is
 * mangled by Unraid's MarkdownExtra pass; external assets are markdown-safe).
 * Vanilla JS — no jQuery dependency. Each .idxcp: swatch chip (opens the native
 * picker) + hex field + preset ribbon + rotating reset. Stores bare 6-hex.
 */
(function(){
  "use strict";
  var PRESETS=['f15a2c','e22828','ff8c2f','3fb950','f0a020','0064ff','00ff00','ffffff','1b1b1b','262626','f2f2f2'];
  var RESET_SVG='<svg viewBox="0 0 24 24" width="13" height="13" fill="none" stroke="currentColor"'+
    ' stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">'+
    '<path d="M3.5 12a8.5 8.5 0 1 0 2.6-6.1"/><path d="M3 4.2v4.6h4.6"/></svg>';
  function norm(raw){ var s=String(raw==null?'':raw).trim().replace(/^#+/,'').toLowerCase();
    if(/^[0-9a-f]{3}$/.test(s)) s=s.charAt(0)+s.charAt(0)+s.charAt(1)+s.charAt(1)+s.charAt(2)+s.charAt(2);
    return /^[0-9a-f]{6}$/.test(s)?s:null; }
  /* Unraid's change-listener also watches text inputs, but enable Apply explicitly too. */
  function markChanged(el){ var f=el.closest('form'); if(f){ var a=f.querySelectorAll('input[value="Apply"]');
    for(var i=0;i<a.length;i++) a[i].disabled=false; } try{ formHasUnsavedChanges=true; }catch(e){} }
  function setup(cp){
    var text=cp.querySelector('.idxcp-hex'), native=cp.querySelector('.idxcp-native'),
        chip=cp.querySelector('.idxcp-chip'), wrap=cp.querySelector('.idxcp-presets'),
        reset=cp.querySelector('.idxcp-reset');
    var def=norm(text.getAttribute('data-default'))||'000000', syncing=false;
    reset.innerHTML=RESET_SVG;
    function paint(hex){ chip.style.background='#'+hex; native.value='#'+hex;
      var dots=wrap.querySelectorAll('.idxcp-dot');
      for(var i=0;i<dots.length;i++){ var sel=dots[i].getAttribute('data-hex')===hex;
        dots[i].classList.toggle('selected',sel); dots[i].setAttribute('aria-pressed',sel?'true':'false'); } }
    function commit(hex){ if(!hex) return; syncing=true; text.value=hex; text.classList.remove('invalid'); paint(hex);
      text.dispatchEvent(new Event('input',{bubbles:true})); text.dispatchEvent(new Event('change',{bubbles:true}));
      syncing=false; markChanged(text); }
    for(var p=0;p<PRESETS.length;p++){ (function(hex){ var b=document.createElement('button');
      b.type='button'; b.className='idxcp-dot'; b.setAttribute('data-hex',hex); b.style.background='#'+hex;
      b.title='#'+hex; b.setAttribute('aria-label','#'+hex); b.setAttribute('aria-pressed','false');
      b.addEventListener('click',function(){ commit(hex); }); wrap.appendChild(b); })(PRESETS[p]); }
    chip.addEventListener('click',function(){ try{ native.click(); }catch(e){} });
    native.addEventListener('input',function(){ commit(norm(native.value)); });
    native.addEventListener('change',function(){ commit(norm(native.value)); });
    text.addEventListener('input',function(){ if(syncing) return; var n=norm(text.value);
      if(n){ text.classList.remove('invalid'); paint(n); markChanged(text); } else text.classList.add('invalid'); });
    text.addEventListener('change',function(){ if(syncing) return; var n=norm(text.value);
      if(n) commit(n); else text.classList.add('invalid'); });
    reset.addEventListener('click',function(){ commit(def); });
    var init=norm(text.value)||def; text.value=init; paint(init);
  }
  window.idxReset=function(cls){
    var hx=document.querySelectorAll('.idxcp-hex.'+cls);
    for(var i=0;i<hx.length;i++){ hx[i].value=(hx[i].getAttribute('data-default')||'').toLowerCase();
      hx[i].dispatchEvent(new Event('change',{bubbles:true})); }
    var sel=document.querySelectorAll('select.'+cls+'[data-default]');
    for(var j=0;j<sel.length;j++){ sel[j].value=sel[j].getAttribute('data-default');
      sel[j].dispatchEvent(new Event('change',{bubbles:true})); markChanged(sel[j]); }
  };
  function initAll(){ var cps=document.querySelectorAll('.idxcp'); for(var i=0;i<cps.length;i++) setup(cps[i]); }
  if(document.readyState==='loading') document.addEventListener('DOMContentLoaded',initAll); else initAll();
})();
