(() => {
  const searchForm = document.getElementById('search-form');
  const searchInput = document.getElementById('search-input');
  const errorEl = document.getElementById('search-error');

  const SECRET_PHRASES = [
    'workshop-B3',
    'le loup est dans la bergerie'
  ].map(s => s.normalize('NFD').replace(/[\u0300-\u036f]/g,'').toLowerCase().trim());

  function normalizeStr(s){
    return s.normalize('NFD').replace(/[\u0300-\u036f]/g,'').toLowerCase().trim();
  }

  function renderFakeResults(query){
    errorEl.textContent = '';
    console.log(`Résultats pour "${query}"`);
  }

  function handleSearchSubmit(e){
    e.preventDefault();
    const raw = searchInput.value || '';
    const normalized = normalizeStr(raw);

    if(SECRET_PHRASES.includes(normalized)){
      window.location.href = 'hidden/login.html';
    } else if(raw.trim() !== ''){
      renderFakeResults(raw);
    } else {
      errorEl.textContent = '';
    }
  }

  if(searchForm) searchForm.addEventListener('submit', handleSearchSubmit);
})();
