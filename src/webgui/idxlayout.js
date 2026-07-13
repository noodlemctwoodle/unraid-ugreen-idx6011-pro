/*
 * plugin/src/webgui/idxlayout.js
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * The dashboard LAYOUT EDITOR. A "Theme" tab (colours + font + text sizes, from
 * the server-rendered #idxtheme block) followed by one tab per content page: an
 * enable toggle and an editable module list (reorder / add / remove / variant).
 * A LIVE PREVIEW on the right is rendered by the real dashboard via preview.php
 * (panel_dash --preview) and reflects the DRAFT theme too, so colour / font /
 * size changes preview before Apply. Catalog + current layouts arrive as
 * data-attributes on #idxlayout (an inline <script> would be mangled by markdown).
 * Vanilla JS; layout-only CSS.
 */
(function(){
  "use strict";
  var BASE = '/plugins/ugreen-idx6011-pro/preview.php';
  var PAGES = [
    { key:'home', KEY:'HOME', title:'Home', idx:0 },
    { key:'overview', KEY:'OVERVIEW', title:'Overview', idx:1 },
    { key:'hardware', KEY:'HARDWARE', title:'Hardware', idx:2 },
    { key:'network', KEY:'NETWORK', title:'Network', idx:3 },
    { key:'disks', KEY:'DISKS', title:'Disks', idx:4 },
    { key:'docker', KEY:'DOCKER', title:'Docker', idx:5 }
  ];
  /* which cfg colour keys map to which preview.php query params */
  var COLQ = { COL_ACCENT:'accent', COL_GRAD_A:'grada', COL_GRAD_B:'gradb', COL_BG:'bg',
               COL_CARD:'card', COL_TEXT:'ctext', COL_OK:'ok', COL_WARN:'warn', COL_BAD:'bad' };
  var CAT = [], state = {}, active = 'theme', root = null, form = null, timer = null, themeEl = null;

  function catMod(id){ for(var i=0;i<CAT.length;i++) if(CAT[i].id===id) return CAT[i]; return null; }
  function parse(s){ var o=[]; (s||'').split(',').forEach(function(t){ t=t.trim(); if(!t)return;
    var c=t.split(':'); o.push({id:c[0], variant:c[1]||''}); }); return o; }
  function build(mods){ return mods.map(function(m){ return m.variant ? m.id+':'+m.variant : m.id; }).join(','); }
  function el(tag, cls, txt){ var e=document.createElement(tag); if(cls)e.className=cls; if(txt!=null)e.textContent=txt; return e; }

  function hidden(name, val){ if(!form) return;
    var e=form.querySelector('input[name="'+name+'"]');
    if(!e){ e=document.createElement('input'); e.type='hidden'; e.name=name; form.appendChild(e); }
    e.value=val; }
  function markChanged(){ if(!form) return;
    var a=form.querySelectorAll('input[value="Apply"]'); for(var i=0;i<a.length;i++) a[i].disabled=false;
    try{ formHasUnsavedChanges=true; }catch(e){} }
  function syncAll(){ PAGES.forEach(function(p){ var s=state[p.key];
    hidden('LAYOUT_'+p.KEY, build(s.mods)); hidden('PAGE_'+p.KEY, s.on?'1':'0'); }); }

  /* draft theme -> preview.php query params (so previews reflect unsaved theme) */
  function themeParams(){
    if(!themeEl) return '';
    var q=[];
    for(var name in COLQ){ var e=themeEl.querySelector('[name="'+name+'"]');
      if(e && e.value) q.push(COLQ[name]+'='+encodeURIComponent(e.value)); }
    ['FONT:font','HEAD_SCALE:head','TEXT_SCALE:text'].forEach(function(pair){
      var kv=pair.split(':'), e=themeEl.querySelector('[name="'+kv[0]+'"]');
      if(e && e.value) q.push(kv[1]+'='+encodeURIComponent(e.value)); });
    return q.join('&');
  }

  function refreshPreview(){
    var img = document.getElementById('idxprev-img'), note = document.getElementById('idxprev-note');
    if(!img) return;
    var pidx, layout;
    if(active==='theme'){ pidx=1; layout=build(state.overview.mods); }   /* Overview shows theme well */
    else { var p=PAGES.filter(function(x){return x.key===active;})[0]; pidx=p.idx; layout=build(state[active].mods); }
    var th = themeParams();
    if(note) note.textContent='rendering…';
    clearTimeout(timer);
    timer = setTimeout(function(){
      img.onload = function(){ if(note) note.textContent=''; };
      img.onerror = function(){ if(note) note.textContent='preview failed'; };
      img.src = BASE+'?page='+pidx+'&layout='+encodeURIComponent(layout)+(th?('&'+th):'')+'&_='+Date.now();
    }, 350);
  }

  function renderTabs(){
    var bar = document.getElementById('idxlayout-tabs'); bar.innerHTML='';
    var th = el('button','idxl-tab'+(active==='theme'?' on':''), 'Theme'); th.type='button';
    th.onclick=function(){ active='theme'; render(); };
    bar.appendChild(th);
    PAGES.forEach(function(p){
      var b = el('button','idxl-tab'+(p.key===active?' on':''), p.title); b.type='button';
      if(!state[p.key].on) b.classList.add('off');
      b.onclick=function(){ active=p.key; render(); };
      bar.appendChild(b);
    });
  }
  function moduleRow(p, m, i){
    var mods = state[p.key].mods, cm = catMod(m.id);
    var row = el('div','idxl-row');
    row.appendChild(el('span','idxl-name', (cm||{label:m.id}).label));
    if(cm && cm.indexed){                                /* per-item: which #? (1-based) */
      var num = el('input','idxl-var'); num.type='number'; num.min='1';
      num.value = String((parseInt(m.variant,10)||0) + 1);
      num.title = (cm.label||m.id) + ' number';
      num.onchange = function(){ var n=Math.max(1, parseInt(num.value,10)||1);
        num.value=String(n); m.variant=String(n-1); changed(); };
      row.appendChild(num);
    } else if(cm && cm.variants && cm.variants.length>1){
      var sel = el('select','idxl-var');
      cm.variants.forEach(function(v){ var o=el('option',null,v); o.value=v;
        if((m.variant||cm.variants[0])===v) o.selected=true; sel.appendChild(o); });
      sel.onchange=function(){ m.variant = (sel.value===cm.variants[0]?'':sel.value); changed(); };
      row.appendChild(sel);
    } else { row.appendChild(el('span','idxl-var-none','')); }
    var up=el('button','idxl-btn','↑'); up.type='button'; up.title='Move up'; up.disabled=i===0;
    up.onclick=function(){ mods.splice(i-1,0,mods.splice(i,1)[0]); changed(); };
    var dn=el('button','idxl-btn','↓'); dn.type='button'; dn.title='Move down'; dn.disabled=i===mods.length-1;
    dn.onclick=function(){ mods.splice(i+1,0,mods.splice(i,1)[0]); changed(); };
    var rm=el('button','idxl-btn idxl-rm','×'); rm.type='button'; rm.title='Remove';
    rm.onclick=function(){ mods.splice(i,1); changed(); };
    row.appendChild(up); row.appendChild(dn); row.appendChild(rm);
    return row;
  }
  function renderEditor(){
    var pane = document.getElementById('idxlayout-pane'); pane.innerHTML='';
    var p = PAGES.filter(function(x){return x.key===active;})[0], s = state[active];
    var tog = el('label','idxl-toggle'); var cb=el('input'); cb.type='checkbox'; cb.checked=s.on;
    cb.onchange=function(){ s.on=cb.checked; changed(); renderTabs(); };
    tog.appendChild(cb); tog.appendChild(el('span',null,' Show this page on the dashboard'));
    pane.appendChild(tog);
    var list = el('div','idxl-list');
    if(s.mods.length===0) list.appendChild(el('div','idxl-empty','No modules — add one below.'));
    s.mods.forEach(function(m,i){ list.appendChild(moduleRow(p, m, i)); });
    pane.appendChild(list);
    var add = el('div','idxl-add'); var asel=el('select','idxl-addsel');
    var ph=el('option',null,'Add module…'); ph.value=''; asel.appendChild(ph);
    CAT.forEach(function(c){ var o=el('option',null,c.label); o.value=c.id; asel.appendChild(o); });
    asel.onchange=function(){ if(!asel.value) return; s.mods.push({id:asel.value, variant:''}); asel.value=''; changed(); };
    add.appendChild(asel); pane.appendChild(add);
  }
  function render(){
    renderTabs();
    var isTheme = active==='theme';
    var pane = document.getElementById('idxlayout-pane');
    if(pane) pane.style.display = isTheme ? 'none' : '';
    if(themeEl) themeEl.style.display = isTheme ? '' : 'none';
    if(!isTheme) renderEditor();
    refreshPreview();
  }
  function changed(){ syncAll(); markChanged(); renderEditor(); refreshPreview(); }
  function onTheme(){ markChanged(); refreshPreview(); }

  function init(){
    root = document.getElementById('idxlayout'); if(!root) return;
    form = root.closest('form');
    try { CAT = JSON.parse(root.getAttribute('data-catalog') || '[]'); } catch(e){ CAT=[]; }
    var ST = {}; try { ST = JSON.parse(root.getAttribute('data-layouts') || '{}'); } catch(e){}
    PAGES.forEach(function(p,i){ state[p.key] = { mods: parse(ST[p.key]), on: ST.on ? !!ST.on[i] : true }; });

    var css = document.createElement('style'); css.textContent =
      '#idxlayout-tabs{display:flex;flex-wrap:wrap;gap:4px;margin-bottom:10px}'+
      '.idxl-tab{padding:5px 12px;border:1px solid #888;border-radius:5px 5px 0 0;background:transparent;cursor:pointer;font:inherit;color:inherit}'+
      '.idxl-tab.on{border-bottom-color:transparent;font-weight:600}'+
      '.idxl-tab.off{opacity:.5;font-style:italic}'+
      '#idxlayout-body{display:flex;gap:22px;align-items:flex-start;flex-wrap:wrap}'+
      '#idxlayout-pane{flex:1 1 320px;min-width:290px}'+
      '#idxtheme{flex:1 1 320px;min-width:290px}'+
      '.idxth-grid{display:grid;grid-template-columns:auto 1fr;gap:9px 12px;align-items:center}'+
      '.idxth-grid label{opacity:.85}'+
      '.idxl-toggle{display:flex;align-items:center;gap:8px;margin:2px 0 14px;cursor:pointer}'+
      '.idxl-list{display:flex;flex-direction:column;gap:6px;margin-bottom:12px}'+
      '.idxl-row{display:flex;align-items:center;gap:8px;padding:6px 8px;border:1px solid rgba(128,128,128,.35);border-radius:6px}'+
      '.idxl-name{flex:1;min-width:80px}.idxl-var{min-width:76px}.idxl-var-none{width:76px}'+
      '.idxl-btn{width:28px;height:28px;border:1px solid #888;border-radius:5px;background:transparent;color:inherit;cursor:pointer;font:inherit;line-height:1}'+
      '.idxl-btn:disabled{opacity:.3;cursor:default}.idxl-rm{color:#c33}'+
      '.idxl-empty{opacity:.6;font-style:italic;padding:6px 2px}'+
      '.idxl-add{margin-bottom:8px}.idxl-addsel{min-width:180px}'+
      '#idxprev-wrap{flex:0 0 auto;text-align:center}'+
      '#idxprev-img{width:170px;height:auto;border:1px solid rgba(128,128,128,.5);border-radius:6px;background:#111;display:block}'+
      '#idxprev-note{font-size:.8em;opacity:.7;height:1.2em;margin-top:4px}'+
      '.idxl-prevlabel{font-size:.8em;opacity:.6;margin-bottom:5px;letter-spacing:.5px;text-transform:uppercase}';
    document.head.appendChild(css);

    root.innerHTML =
      '<div id="idxlayout-tabs"></div>'+
      '<div id="idxlayout-body"><div id="idxlayout-pane"></div>'+
      '<div id="idxprev-wrap"><div class="idxl-prevlabel">Live preview</div>'+
      '<img id="idxprev-img" alt="preview"><div id="idxprev-note"></div></div></div>';

    /* relocate the server-rendered theme controls into the editor body (between the
     * module pane and the preview) so they sit under the Theme tab, and update the
     * preview when they change. They stay inside the form, so Apply still saves them. */
    themeEl = document.getElementById('idxtheme');
    if(themeEl){
      var body = document.getElementById('idxlayout-body');
      body.insertBefore(themeEl, document.getElementById('idxprev-wrap'));
      themeEl.addEventListener('input', onTheme);
      themeEl.addEventListener('change', onTheme);
    }
    syncAll(); render();
  }
  if(document.readyState==='loading') document.addEventListener('DOMContentLoaded', init); else init();
})();
