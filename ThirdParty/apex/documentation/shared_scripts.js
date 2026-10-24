// Hamburger menu functionality
(function() {
  function initHamburgerMenu() {
    var hamburger = document.getElementById('hamburger-menu');
    var sidebar = document.querySelector('.main-toc, .sidebar');
    var overlay = document.getElementById('mobile-menu-overlay');

    if (!hamburger || !sidebar) return;

    function toggleMenu() {
      var isOpen = sidebar.classList.contains('mobile-open');
      if (isOpen) {
        sidebar.classList.remove('mobile-open');
        hamburger.classList.remove('active');
        if (overlay) overlay.classList.remove('active');
      } else {
        sidebar.classList.add('mobile-open');
        hamburger.classList.add('active');
        if (overlay) overlay.classList.add('active');
      }
    }

    hamburger.addEventListener('click', function(e) {
      e.stopPropagation();
      toggleMenu();
    });

    if (overlay) {
      overlay.addEventListener('click', function() {
        toggleMenu();
      });
    }

    // Close menu when clicking on a sidebar link (mobile only)
    if (window.innerWidth <= 768) {
      var sidebarLinks = sidebar.querySelectorAll('a');
      sidebarLinks.forEach(function(link) {
        link.addEventListener('click', function() {
          setTimeout(function() {
            sidebar.classList.remove('mobile-open');
            hamburger.classList.remove('active');
            if (overlay) overlay.classList.remove('active');
          }, 100);
        });
      });
    }

    // Close menu on window resize if going to desktop
    window.addEventListener('resize', function() {
      if (window.innerWidth > 768) {
        sidebar.classList.remove('mobile-open');
        hamburger.classList.remove('active');
        if (overlay) overlay.classList.remove('active');
      }
    });
  }

  // Initialize when DOM is ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initHamburgerMenu);
  } else {
    initHamburgerMenu();
  }
})();
