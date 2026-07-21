# Long-range acoustic localization of artillery shots using distributed synchronous acoustic sensors

## Abstract (přepis)
Acoustic recordings of artillery shots feature the signatures of the shot’s muzzle, projectile, and impact waves modulated by the environment. This study aims at improving the sensing of such shots using a set of synchronous acoustic sensors distributed over a 1 km2 area. It uses the time matching approach, which is based on finding the best match between the observed and pre-calculated times of arrivals of the various waves at each sensor. The pre-calculations introduced here account for the complex acoustic source with a 6-degrees-of-freedom ballistic trajectory model, and for the propagation channel with a wavefront-tracking acoustic model including meteorological and terrain effects. The approach is demonstrated using three recordings of artillery shots measured by sensors which are more than 10 km from the point of fire and distributed at several hundred meters away from and around the target points. Using only the impact wave, it locates the impact point with an error of a few meters. Processing the muzzle and impact and projectile waves enables the estimation of the weapon’s position with a 1 km error. Sensitivities of the localization method to various factors such as the number of sensors, atmospheric data, and the number of processed waves are discussed.

## Co je zajímavé pro náš článek
- Silný referenční základ pro fyzikálně motivované zpracování (TOA + propagace + meteorologie), které může doplnit ML přístup.
- Užitečné pro framing: detekce/klasifikace vs. lokalizace jsou odlišné úlohy s jinou metrikou.
- Důraz na citlivost na atmosférické podmínky podporuje naši argumentaci o robustnosti v reálném prostředí.
