const {Eknr, Gio} = imports.gi;

function render_model_with_options(renderer,
    html, model, use_scroll_manager=false, show_title=false) {
    return renderer.render_legacy_content(html,
        model.source, model.source_name, model.original_uri,
        model.license, model.title, show_title, use_scroll_manager);
}

describe('Legacy HTML Renderer', function () {
    let wikihow_model, wikibooks_model, wikipedia_model, wikisource_model;
    let all_models;
    let renderer;

    const html = '<html><body><p>dummy html</p></body></html>';

    beforeEach(function () {
        spyOn(Gio.Application, 'get_default').and.returnValue({
            get_web_overrides_css: function () { return []; },
        });

        renderer = new Eknr.Renderer();
        wikipedia_model = {
            source_uri: 'http://en.wikipedia.org/wiki/When_It_Hits_the_Fan',
            original_uri: 'http://en.wikipedia.org/wiki/When_It_Hits_the_Fan',
            content_type: 'text/html',
            source: 'wikipedia',
            source_name: 'Wikipedia',
            license: 'CC-BY-SA 3.0',
            title: 'Wikipedia title',
            is_server_templated: false,
        };
        wikisource_model = {
            source_uri: 'http://en.wikisource.org/wiki/When_It_Hits_the_Fan',
            original_uri: 'http://en.wikisource.org/wiki/When_It_Hits_the_Fan',
            content_type: 'text/html',
            source: 'wikisource',
            source_name: 'Wikibooks',
            license: 'CC-BY-SA 3.0',
            title: 'Wikibooks title',
            is_server_templated: false,
        };
        wikihow_model = {
            source_uri: 'http://www.wikihow.com/Give-Passive-Aggressive-Gifts-for-Christmas',
            original_uri: 'http://www.wikihow.com/Give-Passive-Aggressive-Gifts-for-Christmas',
            content_type: 'text/html',
            source: 'wikihow',
            source_name: 'wikiHow',
            license: 'Owner permission',
            title: 'Wikihow & title',
            is_server_templated: false,
        };
        wikibooks_model = {
            source_uri: 'http://en.wikibooks.org/wiki/When_It_Hits_the_Fan',
            original_uri: 'http://en.wikibooks.org/wiki/When_It_Hits_the_Fan',
            content_type: 'text/html',
            source: 'wikibooks',
            source_name: 'Wikibooks',
            license: 'CC-BY-SA 3.0',
            title: 'Wikibooks title',
            is_server_templated: false,
        };

        all_models = [
            wikipedia_model,
            wikisource_model,
            wikihow_model,
            wikibooks_model,
        ];
    });

    it('can render an article', function () {
        expect(render_model_with_options(renderer,
            html,
            wikihow_model)).toBeDefined();
    });

    it('shows a title only when told to', function () {
        let html_no_title = render_model_with_options(renderer, html,
            wikibooks_model, false);
        let html_with_title = render_model_with_options(renderer, html,
            wikibooks_model, false, true);
        expect(html_with_title).toMatch('<h1>Wikibooks title</h1>');
        expect(html_no_title).not.toMatch('<h1>Wikibooks title</h1>');
    });

    it('links to creative commons license on wikimedia pages', function () {
        let rendered_html = render_model_with_options(renderer, html, wikibooks_model);
        expect(rendered_html).toMatch('license://CC-BY-SA%203.0');
    });

    it('links to original wikihow articles', function () {
        let rendered_html = render_model_with_options(renderer, html, wikihow_model);
        expect(rendered_html).toMatch('http://www.wikihow.com/Give-Passive-Aggressive-Gifts-for-Christmas');
    });

    it('includes correct css for article type', function () {
        expect(render_model_with_options(renderer, html,
            wikihow_model)).toMatch('wikihow.css');
        expect(render_model_with_options(renderer, html,
            wikibooks_model)).toMatch('wikimedia.css');
    });

    it('escapes html special characters in title', function () {
        let rendered_html = render_model_with_options(renderer, html, wikihow_model);
        expect(rendered_html).toMatch('Wikihow &amp; title');
    });

    it('includes article html unescaped', function () {
        let rendered_html = render_model_with_options(renderer, html, wikihow_model);
        expect(rendered_html).toMatch('<p>dummy html</p>');
    });

    it('includes scroll_manager.js only when told to', function () {
        let html_without_scroll_manager = render_model_with_options(renderer,
            html, wikibooks_model);
        renderer.enable_scroll_manager = true;
        let html_with_scroll_manager = render_model_with_options(renderer,
            html, wikibooks_model, true);

        expect(html_with_scroll_manager).toMatch('scroll-manager.js');
        expect(html_without_scroll_manager).not.toMatch('scroll-manager.js');
    });

    it('includes MathJax in rendered Wikipedia, Wikibooks, and Wikisource articles', function () {
        let rendered_html = render_model_with_options(renderer, html, wikibooks_model);
        expect(rendered_html).toMatch('<script type="text/x-mathjax-config">');
        rendered_html = render_model_with_options(renderer, html, wikipedia_model);
        expect(rendered_html).toMatch('<script type="text/x-mathjax-config">');
        rendered_html = render_model_with_options(renderer, html, wikisource_model);
        expect(rendered_html).toMatch('<script type="text/x-mathjax-config">');
    });

    it('does not include MathJax in articles from other sources', function () {
        let rendered_html = render_model_with_options(renderer, html, wikihow_model);
        expect(rendered_html).not.toMatch('<script type="text/x-mathjax-config">');
    });
});
