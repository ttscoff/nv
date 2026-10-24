## MultiMarkdown image attributes - inline

Expected: `<img src="/images/test-inline-1.jpg" alt="Inline no title" width="200">`

![Inline no title](/images/test-inline-1.jpg width=200)

Expected: `<img src="/images/test-inline-2.jpg" alt="Inline with title" title="Falafel" width="300">`

![Inline with title](/images/test-inline-2.jpg "Falafel" width=300)

Expected: `<img src="/images/test-inline-3.jpg" alt="Inline percent" style="width: 50%">`

![Inline percent](/images/test-inline-3.jpg width=50%)

Expected: `<img src="/images/test-inline-4.jpg" alt="Inline classes" class="center shadow" width="250" style="height: 60%">`

![Inline classes](/images/test-inline-4.jpg "Caption" class=center shadow width=250 height=60%)

## MultiMarkdown image attributes - reference style

Expected: `<img src="/images/test-ref-1.jpg" alt="Ref with attrs" width="200">`

![Ref with attrs][ref-inline-1]

[ref-inline-1]: /images/test-ref-1.jpg width=200

Expected: `<img src="/images/test-ref-2.jpg" alt="Ref with title" title="Falafel" width="300">`

![Ref with title][ref-inline-2]

[ref-inline-2]: /images/test-ref-2.jpg "Falafel" width=300

Expected: `<img src="/images/test-ref-3.jpg" alt="Ref percent" style="width: 50%">`

![Ref percent][ref-inline-3]

[ref-inline-3]: /images/test-ref-3.jpg width=50%

Expected: `<img src="/images/test-ref-4.jpg" alt="Ref classes" class="center shadow" width="250" style="height: 60%">`

![Ref classes][ref-inline-4]

[ref-inline-4]: /images/test-ref-4.jpg "Caption" class=center shadow width=250 height=60%

## @2x (retina srcset)

Expected: `srcset="img/icon.png 1x, img/icon@2x.png 2x"` (and src="img/icon.png")

![BlogBook](img/icon.png @2x)

Expected: @2x with title - srcset present

![BlogBook](img/icon.png "Hero" @2x)

Expected: reference-style @2x - srcset present

![Logo][logo-2x]

[logo-2x]: img/hero.png @2x

