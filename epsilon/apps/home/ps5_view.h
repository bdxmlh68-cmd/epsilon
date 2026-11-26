// apps/home/ps5_view.h  — NOVA OS : accueil style PS5 (fork d'Epsilon)
// Écran NumWorks : 320 x 222 px.
#ifndef HOME_PS5_VIEW_H
#define HOME_PS5_VIEW_H

#include <escher/view.h>
#include <kandinsky/color.h>
#include <kandinsky/context.h>
#include <kandinsky/font.h>

namespace Home {

class PS5View : public Escher::View {
 public:
  PS5View() : m_selected(0), m_count(0), m_names(nullptr) {}

  // names : tableau de noms d'applis (ex. "Calcul", "Graphique", ...)
  void setApps(int count, const char* const* names) {
    m_count = count;
    m_names = names;
    markRectAsDirty(bounds());
  }
  int selected() const { return m_selected; }
  void setSelected(int i) {
    if (i < 0 || i >= m_count || i == m_selected) return;
    m_selected = i;
    markRectAsDirty(bounds());
  }

  void drawRect(KDContext* ctx, KDRect rect) const override {
    const int W = Ion::Display::Width;   // 320
    const int H = Ion::Display::Height;  // 222

    // 1) Fond dégradé bleu nuit (haut -> bas), comme le menu PS5
    for (int y = 0; y < H; y += 2) {
      int r = 8 - (8 * y) / H;
      int g = 30 - (24 * y) / H;
      int b = 77 - (60 * y) / H;
      ctx->fillRect(KDRect(0, y, W, 2), KDColor::RGB888(r, g, b));
    }

    // 2) Barre du haut
    ctx->drawString("NOVA OS", KDPoint(10, 8),
                    KDGlyph::Style{.glyphColor = KDColorWhite,
                                   .backgroundColor = KDColor::RGB888(8, 30, 77),
                                   .font = KDFont::Size::Small});

    if (m_count == 0 || m_names == nullptr) return;

    // 3) Carrousel horizontal : tuile sélectionnée agrandie au centre
    static const uint32_t palette[] = {0x0070D1, 0xE63946, 0x2A9D8F, 0xF4A261,
                                       0x8E44AD, 0x16A085, 0xE67E22, 0x2C3E50};
    const int cy = 100;
    for (int i = 0; i < m_count; i++) {
      bool sel = (i == m_selected);
      int size = sel ? 84 : 56;
      int cx = W / 2 + (i - m_selected) * 78;
      if (i > m_selected) cx += 14;
      if (i < m_selected) cx -= 14;
      if (cx + size / 2 < 0 || cx - size / 2 > W) continue;
      int x = cx - size / 2, y = cy - size / 2;
      KDColor c = KDColor::RGB24(palette[i % 8]);
      if (sel) {  // halo blanc façon PS5
        ctx->fillRect(KDRect(x - 3, y - 3, size + 6, size + 6), KDColorWhite);
      }
      ctx->fillRect(KDRect(x, y, size, size), c);
      // initiale de l'appli au centre de la tuile
      char s[2] = {m_names[i][0], 0};
      ctx->drawString(s, KDPoint(cx - 5, cy - 9),
                      KDGlyph::Style{.glyphColor = KDColorWhite,
                                     .backgroundColor = c,
                                     .font = sel ? KDFont::Size::Large
                                                 : KDFont::Size::Small});
    }

    // 4) Nom de l'appli sélectionnée sous la tuile
    const char* name = m_names[m_selected];
    int len = 0;
    while (name[len]) len++;
    ctx->drawString(name, KDPoint(W / 2 - len * 5, 170),
                    KDGlyph::Style{.glyphColor = KDColorWhite,
                                   .backgroundColor = KDColor::RGB888(8, 20, 50),
                                   .font = KDFont::Size::Large});
  }

 private:
  int m_selected;
  int m_count;
  const char* const* m_names;
};

}  // namespace Home

#endif
