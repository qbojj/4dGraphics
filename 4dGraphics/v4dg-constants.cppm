export module v4dg:constants;

import glm;

export namespace v4dg::constants {
const glm::vec4 vGrey(192 / 255.F, 192 / 255.F, 192 / 255.F, 1),
    vDarkGrey(0.5, 0.5, 0.5, 1), vVeryDarkGrey(0.25, 0.25, 0.25, 1),
    vRed(1, 0, 0, 1), vDarkRed(0.5, 0, 0, 1), vVeryDarkRed(0.25, 0, 0, 1),
    vYellow(1, 1, 0, 1), vDarkYellow(0.5, 0.5, 0, 1),
    vVeryDarkYellow(0.25, 0.25, 0, 1), vGreen(0, 1, 0, 1),
    vDarkGreen(0, 0.5, 0, 1), vVeryDarkGreen(0, 0.25, 0, 1), vCyan(0, 1, 1, 1),
    vDarkCyan(0, 0.5, 0.5, 1), vVeryDarkCyan(0, 0.25, 0.25, 1),
    vBlue(0, 0, 1, 1), vDarkBlue(0, 0, 0.5, 1), vVeryDarkBlue(0, 0, 0.25, 1),
    vMagenta(1, 0, 1, 1), vDarkMagenta(0.5, 0, 0.5, 1),
    vVeryDarkMagenta(0.25, 0, 0.25, 1), vWhite(1, 1, 1, 1), vBlack(0, 0, 0, 1),
    vBlank(0, 0, 0, 0);

const glm::vec3 vUp(0, 1, 0), vDown(0, -1, 0), vRight(1, 0, 0), vLeft(-1, 0, 0),
    vFront(0, 0, -1), vBack(0, 0, 1), vZero(0, 0, 0), vOne(1, 1, 1);
} // namespace v4dg::constants
