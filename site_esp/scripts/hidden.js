(() => {
  const DEMO_USER = 'editor';
  const DEMO_PASS = 'correcthorsebatterystaple'; 

  function createEl(tag, attrs={}, children=[]){
    const el = document.createElement(tag);
    for(const k in attrs){
      if(k === 'class') el.className = attrs[k];
      else if(k === 'html') el.innerHTML = attrs[k];
      else el.setAttribute(k, attrs[k]);
    }
    children.forEach(c => el.appendChild(typeof c === 'string' ? document.createTextNode(c) : c));
    return el;
  }

  let modal = null;
  function buildModal(){
    if(modal) return modal;
    modal = createEl('div', { id:'hidden-modal', class:'hidden-modal', style:'display:flex;position:fixed;inset:0;align-items:center;justify-content:center;background:rgba(0,0,0,0.5);z-index:9999;' });
    const panel = createEl('div', { class:'hidden-panel', style:'width:380px;max-width:94%;background:#fff;border-radius:12px;padding:1rem;box-shadow:0 14px 40px rgba(0,0,0,.25)' });
    const title = createEl('h3', {}, ['Espace réservé']);
    const p = createEl('p', { style:'color:#666;margin:0 0 .6rem' }, ['Identifiez-vous pour accéder au contenu privé.']);
    const form = createEl('form', { id:'hidden-auth-form' });
    const inpU = createEl('input', { name:'username', placeholder:"Nom d'utilisateur", autocomplete:'username', style:'width:100%;padding:.6rem;border:1px solid #e6e6e6;border-radius:8px;margin-top:.4rem' });
    const inpP = createEl('input', { name:'password', type:'password', placeholder:'Mot de passe', autocomplete:'current-password', style:'width:100%;padding:.6rem;border:1px solid #e6e6e6;border-radius:8px;margin-top:.4rem' });
    const err = createEl('div', { class:'hidden-error', style:'color:#b31a2e;margin-top:.5rem;height:1.1em' }, ['']);
    const row = createEl('div', { style:'display:flex;gap:.5rem;margin-top:.75rem;justify-content:flex-end' });
    const submit = createEl('button', { type:'submit', style:'padding:.55rem .8rem;border-radius:8px;border:0;background:#004170;color:#fff;cursor:pointer' }, ['Se connecter']);
    const cancel = createEl('button', { type:'button', style:'padding:.55rem .8rem;border-radius:8px;border:0;background:#f3f6fb;cursor:pointer;margin-right:auto' }, ['Annuler']);
    row.appendChild(cancel);
    row.appendChild(submit);
    form.appendChild(inpU);
    form.appendChild(inpP);
    form.appendChild(err);
    form.appendChild(row);
    panel.appendChild(title);
    panel.appendChild(p);
    panel.appendChild(form);
    modal.appendChild(panel);

    cancel.addEventListener('click', hideModal);
    modal.addEventListener('click', (e)=> { if(e.target === modal) hideModal(); });
    document.addEventListener('keydown', (e)=> { if(e.key === 'Escape') hideModal(); });

    form.addEventListener('submit', (ev)=> {
      ev.preventDefault();
      err.textContent = '';
      const u = inpU.value.trim();
      const pass = inpP.value;
      if(!u || !pass){ err.textContent = 'Remplissez tous les champs.'; return; }
      if(u === DEMO_USER && pass === DEMO_PASS){
        hideModal();
        window.location.href = '/hidden/news.html';
        return;
      } else {
        err.textContent = 'Identifiants incorrects.';
      }
    });

    return modal;
  }

  function showModal(hint){
    const m = buildModal();
    if(!document.body.contains(m)) document.body.appendChild(m);
    m.style.display = 'flex';
    if(hint){
      const u = m.querySelector('input[name="username"]');
      if(u) u.value = hint;
    }
    const first = m.querySelector('input[name="username"]');
    if(first) first.focus();
  }

  function hideModal(){
    if(!modal) return;
    modal.style.display = 'none';
  }

  window.addEventListener('requestHiddenModal', (ev) => {
    const hint = ev && ev.detail && ev.detail.hint ? ev.detail.hint : undefined;
    showModal(hint);
  });
  window.HiddenModal = { show: showModal, hide: hideModal };
  const help = document.getElementById('help-trigger');
  if(help) help.addEventListener('click', (e)=> { e.preventDefault(); showModal(); });

  console.info('hidden.js ready — modal injectée à la demande (prototype).');
})();
